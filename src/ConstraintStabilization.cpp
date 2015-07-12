/****************************************************************************
 * Copyright 2015 Evan Drumwright
 * This library is distributed under the terms of the Apache V2.0 
 * License (obtainable from http://www.apache.org/licenses/LICENSE-2.0).
 ****************************************************************************/

#include <map>
#include <Moby/Types.h>
#include <Moby/EventDrivenSimulator.h>
#include <Moby/ConstraintStabilization.h>

using namespace Ravelin;
using namespace Moby;
using boost::weak_ptr;
using boost::shared_ptr;
using boost::shared_array;
using boost::const_pointer_cast; 
using boost::dynamic_pointer_cast; 
using std::map;
using std::vector;
using std::make_pair;
using std::endl;
using std::list;

ConstraintStabilization::ConstraintStabilization(shared_ptr<EventDrivenSimulator> sim)
{
  _sim = sim;

  // set tolerance to NEAR_ZERO by default
  eps = NEAR_ZERO;
}

/// Gets the minimum pairwise distance
double ConstraintStabilization::get_min_pairwise_dist(const vector<PairwiseDistInfo>& pdi)
{
  // set distance to infinite initially
  double dist = std::numeric_limits<double>::max();

  for (unsigned i=0; i< pdi.size(); i++)
    dist = std::min(dist, pdi[i].dist);

  return dist;
}

/// Stabilizes the constraints in the simulator
void ConstraintStabilization::stabilize()
{
  VectorNd dq, q;
  std::vector<UnilateralConstraintProblemData> pd;

  std::map<DynamicBodyPtr, unsigned> body_index_map;

  get_body_configurations(q);
  generate_body_index_map(body_index_map);
  // get the pairwise distances
  vector<PairwiseDistInfo>& pdi = _sim->_pairwise_distances;

  // see whether any pairwise distances are below epsilon
  double min_dist = get_min_pairwise_dist(pdi);
  while (min_dist < eps)
  {
    // compute problem data (get M, N, alpha, etc.) 
    compute_problem_data(pd);

    // determine dq's
    dq.set_zero(q.size());
    for (unsigned i=0; i< pd.size(); i++)
      determine_dq(pd[i], dq, body_index_map);

    // determine s and update q 
    update_q(dq, q);

    // update minimum distance
    min_dist = get_min_pairwise_dist(pdi);  
  }
}



void ConstraintStabilization::add_articulate_limit_constraint(std::vector<UnilateralConstraintProblemData>& pd_vector, ArticulatedBodyPtr ab)
{
  std::vector<UnilateralConstraint> limits = new std::vector<UnilateralConstraint>();
  OutputIterator result = limits.begin();
  ab.find_limits(result);
  pd_vector.insert(pd_vector.end(), contacts.begin(), contacts.end());
}

void ConstraintStabilization::add_contact_constraints(std::vector<UnilateralConstraintProblemData>& pd_vector, RigidBodyPtr rb1, RigidBodyPtr rb2)
{
  Point3D::p1, p2;
  std::list<CollisionGeometryPtr> cgs1 = rb1->geometries;
  std::list<CollisionGeometryPtr> cgs2 = rb2->geometries;
  BOOST_FOREACH(CollisionGeometryPtr* cg1 , cgs1)
  {

    BOOST_FOREACH(CollisionGeometryPtr* cg2, cgs2)
    {
      double dist = CollisionGeometry::calc_signed_dist(*cg1, *cg2, p1, p2);
      if(dist < 0)
      {
        //TODO: Add constraints generation code

        /*
        * 1. calc_Minkowski_difference
        * 2. check where the origin (0,0,0) lies in the polygon (Polyhedron::inside_or_on) 
        * 3. find closest feature to the origin
        * 4. the contact limit will be the distance vector from the face to the point
        */
      }
    }
  }
}

/// Computes the constraint data
void ConstraintStabilization::compute_problem_data(std::vector<UnilateralConstraintProblemData>& pd_vector)
{
  std::vector<UnilateralConstraint> constraints;

  // clear the problem data vector 
  pd_vector.clear();

  // get all bodies
  std::vector<DynamicBodyPtr> bodies = _sim->_bodies;
  // TODO: Evan add sweep and prune

  // 1) for each pair of bodies in kissing contact, add as many 
  //    UnilateralConstraint objects to constraints as there are 
  //    points of contact between the bodies 
  //    (call _sim->_coldet->find_contacts(.))
  std::vector<UnilateralConstraint> contacts = _sim->coldet->find_constacts();
  pd_vector.insert(pd_vector.end(), contacts.begin(), contacts.end());
  
  BOOST_FOREACH(DynamicBodyPtr* D_body1, bodies)
  {
    RigidBodyPtr rb1, rb2;
    
    if(rb1 = boost::dynamic_pointer_cast<RigidBody>(*D_body1))
    {
      BOOST_FOREACH(DynamicBodyPtr* D_body2, bodies)
      {
        // if the two pointer points to the same body, then no need to add contact
        if(*D_body1 == *D_body2)
        {
          continue;
        }

        //RigidBody
        if(rb2 = boost::dynamic_pointer_cast<RigidBody>(*D_body2))
        {
          add_contact_constraints(pd_vector, rb1,rb2);
        }
        else
        {
          ArticulatedBodyPtr ab2 = dynamic_pointer_cast<ArticulatedBody>(*D_body2);
          add_articulate_limit_constraint(pd_vector, ab2);
          std::vector<RigidBodyPtr> ls2 = ab2->get_links();
          BOOST_FOREACH(RigidBodyPtr* l2, ls2)
          {
            rb2 = dynamic_pointer_cast<RigidBody> (*l2);
            add_contact_constraints(pd_vector, rb1, rb2);
          }
        }
      }
    }
    // body 1 is a articulated body
    else
    {
      ArticulatedBodyPtr ab1 = dynamic_pointer_cast<ArticulatedBody>(*D_body1);
      add_articulate_limit_constraint(pd_vector, ab1);
      std::vector<RigidBodyPtr> ls1 = ab1->get_links();

      BOOST_FOREACH(DynamicBodyPtr* D_body2, bodies)
      {
        // if the two pointer points to the same body, then no need to add contact
        if(*D_body1 == *D_body2)
        {
          continue;
        }

        // since the two pointer are not pointing to the same body, it is ok to start iterating through the first  
        BOOST_FOREACH(RigidBodyPtr* l1, ls1)
        {
          rb1 = *l1;
          //RigidBody
          if(rb2 = boost::dynamic_pointer_cast<RigidBody>(*D_body2))
          {
            add_contact_constraints(pd_vector, rb1,rb2);
          }
          else
          {
            ArticulatedBodyPtr ab2 = dynamic_pointer_cast<ArticulatedBody>(*D_body2);
            add_articulate_limit_constraint(pd_vector, ab2);
            std::vector<RigidBodyPtr> ls2 = ab2->joints;
            BOOST_FOREACH(RigidBodyPtr* l2, ls2)
            {
              rb2 = *l2;
              add_contact_constraints(pd_vector, rb1, rb2);
            }
          }
        }
      }
    }
  }

  // 2) for each articulated body, add as many UnilateralConstraint objects as
  //    there are joints at their limits


  // 3) for each pair of bodies in interpenetrating contact, add a single
  //    point of contact at the deepest interpenetrating point with normal
  //    in the direction of the signed distance function. 

  // find islands
  list<list<UnilateralConstraint*> > islands;
  UnilateralConstraint::determine_connected_constraints(constraints, islands);

  // process islands
  BOOST_FOREACH(list<UnilateralConstraint*>& island, islands)
  {
    // setup a UnilateralConstraintProblemData object
    pd_vector.push_back(UnilateralConstraintProblemData());
    UnilateralConstraintProblemData& pd = pd_vector.back();

    // put the constraint into the appropriate place
    BOOST_FOREACH(UnilateralConstraint* c, island)
    { 
      if (c->constraint_type == UnilateralConstraint::eContact)
        pd.contact_constraints.push_back(c);
      else
        pd.limit_constraints.push_back(c);
    }

    // set number of contact and limit constraints
    pd.N_CONTACTS = pd.contact_constraints.size();
    pd.N_LIMITS = pd.limit_constraints.size(); 

    // now set the unilateral constraint data
    set_unilateral_constraint_data(pd);

    
    // set the elements of Cn_v and L_v
    // L_v is always set to zero
    // Cn_v is set to the signed distance between the two bodies
    for (int i = 0; i < pd.contact_constraints.size())
    {
      pd.Cn_v[i] = CollisionGeometry::calc_signed_dist(pd.contact_constraints[i] -> contact_geom1, pd.contact_constraints[i] -> pd.contact_geom2);
    }

  }
}

/// Gets the super body (articulated if any)
DynamicBodyPtr ConstraintStabilization::get_super_body(SingleBodyPtr sb)
{
  ArticulatedBodyPtr ab = sb->get_articulated_body();
  if (ab)
    return ab;
  else
    return sb;
}

/// Computes the data to the LCP / QP problems
void ConstraintStabilization::set_unilateral_constraint_data(UnilateralConstraintProblemData& pd)
{
  const unsigned UINF = std::numeric_limits<unsigned>::max();
  MatrixNd MM;
  VectorNd v;

  // determine set of "super" bodies from contact constraints
  pd.super_bodies.clear();
  for (unsigned i=0; i< pd.contact_constraints.size(); i++)
  {
    pd.super_bodies.push_back(get_super_body(pd.contact_constraints[i]->contact_geom1->get_single_body()));
    pd.super_bodies.push_back(get_super_body(pd.contact_constraints[i]->contact_geom2->get_single_body()));
  }

  // determine set of "super" bodies from limit constraints
  for (unsigned i=0; i< pd.limit_constraints.size(); i++)
  {
    RigidBodyPtr outboard = pd.limit_constraints[i]->limit_joint->get_outboard_link();
    pd.super_bodies.push_back(get_super_body(outboard));
  }

  // make super bodies vector unique
  std::sort(pd.super_bodies.begin(), pd.super_bodies.end());
  pd.super_bodies.erase(std::unique(pd.super_bodies.begin(), pd.super_bodies.end()), pd.super_bodies.end());

  // set total number of generalized coordinates
  pd.N_GC = 0;
  for (unsigned i=0; i< pd.super_bodies.size(); i++)
    pd.N_GC += pd.super_bodies[i]->num_generalized_coordinates(DynamicBody::eSpatial);

  // initialize constants and set easy to set constants
  pd.N_CONTACTS = pd.contact_constraints.size();
  pd.N_LIMITS = pd.limit_constraints.size();

  // setup constants related to articulated bodies
  for (unsigned i=0; i< pd.super_bodies.size(); i++)
  {
    ArticulatedBodyPtr abody = dynamic_pointer_cast<ArticulatedBody>(pd.super_bodies[i]);
    if (abody) {
      pd.N_CONSTRAINT_EQNS_IMP += abody->num_constraint_eqns_implicit();
    }
  }

  // setup number of friction polygon edges / true cones
  pd.N_K_TOTAL = 0;
  pd.N_LIN_CONE = 0;
  pd.N_TRUE_CONE = 0; 

  // initialize the problem matrices / vectors
  pd.Cn_iM_CnT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Cn_iM_CsT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Cn_iM_CtT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Cn_iM_LT.set_zero(pd.N_CONTACTS, pd.N_LIMITS);
  pd.Cn_iM_JxT.set_zero(pd.N_CONTACTS, pd.N_CONSTRAINT_EQNS_IMP);
  pd.Cs_iM_CsT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Cs_iM_CtT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Cs_iM_LT.set_zero(pd.N_CONTACTS, pd.N_LIMITS);
  pd.Cs_iM_JxT.set_zero(pd.N_CONTACTS, pd.N_CONSTRAINT_EQNS_IMP);
  pd.Ct_iM_CtT.set_zero(pd.N_CONTACTS, pd.N_CONTACTS);
  pd.Ct_iM_LT.set_zero(pd.N_CONTACTS, pd.N_LIMITS);
  pd.Ct_iM_JxT.set_zero(pd.N_CONTACTS, pd.N_CONSTRAINT_EQNS_IMP);
  pd.L_iM_LT.set_zero(pd.N_LIMITS, pd.N_LIMITS);
  pd.L_iM_JxT.set_zero(pd.N_LIMITS, pd.N_CONSTRAINT_EQNS_IMP);
  pd.Jx_iM_JxT.set_zero(pd.N_CONSTRAINT_EQNS_IMP, pd.N_CONSTRAINT_EQNS_IMP);
  pd.Cn_v.set_zero(pd.N_CONTACTS);
  pd.Cs_v.set_zero(pd.N_CONTACTS);
  pd.Ct_v.set_zero(pd.N_CONTACTS);
  pd.L_v.set_zero(pd.N_LIMITS);
  pd.Jx_v.set_zero(pd.N_CONSTRAINT_EQNS_IMP);
  pd.cn.set_zero(pd.N_CONTACTS);
  pd.cs.set_zero(pd.N_CONTACTS);
  pd.ct.set_zero(pd.N_CONTACTS);
  pd.l.set_zero(pd.N_LIMITS);
  pd.alpha_x.set_zero(pd.N_CONSTRAINT_EQNS_IMP);

  // setup indices
  pd.CN_IDX = 0;
  pd.CS_IDX = pd.CN_IDX + pd.N_CONTACTS;
  pd.CT_IDX = pd.CS_IDX;
  pd.NCS_IDX = pd.CS_IDX;
  pd.NCT_IDX = pd.CS_IDX;
  pd.L_IDX = pd.CS_IDX;
  pd.ALPHA_X_IDX = pd.L_IDX + pd.N_LIMITS;
  pd.N_VARS = pd.ALPHA_X_IDX + pd.N_CONSTRAINT_EQNS_IMP;

  // get iterators to the proper matrices
  RowIteratord CnCn = pd.Cn_iM_CnT.row_iterator_begin();
  RowIteratord CnCs = pd.Cn_iM_CsT.row_iterator_begin();
  RowIteratord CnCt = pd.Cn_iM_CtT.row_iterator_begin();
  RowIteratord CsCs = pd.Cs_iM_CsT.row_iterator_begin();
  RowIteratord CsCt = pd.Cs_iM_CtT.row_iterator_begin();
  RowIteratord CtCt = pd.Ct_iM_CtT.row_iterator_begin();

  // process contact constraints, setting up matrices
  for (unsigned i=0; i< pd.contact_constraints.size(); i++)
  {
    // compute cross constraint data for contact constraints
    for (unsigned j=0; j< pd.contact_constraints.size(); j++)
    {
      // reset MM
      MM.set_zero(3, 3);

      // check whether i==j (single contact constraint)
      if (i == j)
      {
        // compute matrix / vector for contact constraint i
        v.set_zero(3);
        pd.contact_constraints[i]->compute_constraint_data(MM, v);

        // setup appropriate part of contact inertia matrices
        RowIteratord_const data = MM.row_iterator_begin();
        *CnCn = *data++;
      }

      // advance the iterators
      CnCn++;
    }

    // compute cross constraint data for contact/limit constraints
    for (unsigned j=0; j< pd.limit_constraints.size(); j++)
    {
      // reset MM
      MM.set_zero(3, 1);

      // compute matrix for cross constraint
      pd.contact_constraints[i]->compute_cross_constraint_data(*pd.limit_constraints[j], MM);

      // setup appropriate parts of contact / limit inertia matrices
      ColumnIteratord_const data = MM.column_iterator_begin();
      pd.Cn_iM_LT(i,j) = *data++;
    }
  }

  // process limit constraints, setting up matrices
  for (unsigned i=0; i< pd.limit_constraints.size(); i++)
  {
    // compute matrix / vector for contact constraint i
    pd.limit_constraints[i]->compute_constraint_data(MM, v);

    // setup appropriate entry of limit inertia matrix and limit velocity
    pd.L_iM_LT(i,i) = MM.data()[0];

    // compute cross/cross limit constraint data
    for (unsigned j=i+1; j< pd.limit_constraints.size(); j++)
    {
      // reset MM
      MM.resize(1,1);

      // compute matrix for cross constraint
      pd.limit_constraints[i]->compute_cross_constraint_data(*pd.limit_constraints[j], MM);

      // setup appropriate part of limit / limit inertia matrix
      pd.L_iM_LT(i,j) = pd.L_iM_LT(j,i) = MM.data()[0];
    }

    // NOTE: cross data has already been computed for contact/limit constraints
  }
}

/// Computes deltaq by solving a linear complementarity problem
void ConstraintStabilization::determine_dq(const UnilateralConstraintProblemData& pd, VectorNd& dqm, std::map<DynamicBodyPtr, unsigned> body_index_map)
{
  VectorNd dq_sub;

  // initialize the LCP matrix and LCP vector
  MatrixNd MM(pd.N_CONTACTS + pd.N_LIMITS, pd.N_CONTACTS + pd.N_LIMITS);
  VectorNd qq(pd.N_CONTACTS + pd.N_LIMITS);

  // setup the LCP matrix and LCP vector
  MM.block(0, pd.N_CONTACTS, 0, pd.N_CONTACTS) = pd.Cn_iM_CnT;
  MM.block(0, pd.N_CONTACTS, pd.N_CONTACTS, MM.columns()) = pd.Cn_iM_LT;
  SharedMatrixNd L_iM_CnT_block = MM.block(pd.N_CONTACTS, MM.rows(), 0, pd.N_CONTACTS);
  MatrixNd::transpose(pd.Cn_iM_LT, L_iM_CnT_block);
  MM.block(pd.N_CONTACTS, MM.rows(), pd.N_CONTACTS, MM.columns()) = pd.L_iM_LT;
  qq.segment(0, pd.N_CONTACTS) = pd.Cn_v;
  qq.segment(pd.N_CONTACTS, qq.size()) = pd.L_v;

  // solve N*inv(M)*N'*dq = N*alpha for dq_sub
  if (!_lcp.lcp_fast(MM, qq, dq_sub))
    _lcp.lcp_lemke_regularized(MM, qq, dq_sub);


  // populating dq based on dq_sub
  unsigned last = 0;
  for(unsigned i = 0; i< pd.super_bodies.size(); i++)
  {
    unsigned start = (body_index_map.find(pd.super_bodies[i]))->second;
    unsigned coord_num = pd.super_bodies[i]->num_generalized_coordinates(DynamicBody eEuler);
    for(unsigned j = 0; j < coord_num; j++))
    {
      dqm[start+j] = dq_sub[last+j];
    }
    last += coord_num;
  }

}

/// Updates q doing a backtracking line search
void ConstraintStabilization::update_q(const VectorNd& dq, VectorNd& q)
{
  VectorNd qstar;

  // get the pairwise distances
  vector<PairwiseDistInfo>& pdi = _sim->_pairwise_distances;

  // setup BLS parameters
  const double ALPHA = 0.05, BETA = 0.8;
  double t = 1.0;

  // compute s at current configuration
  double s0 = compute_s(pdi); 

  // compute qstar 
  qstar = dq;
  qstar *= t;
  qstar += q;

  // update body configurations
  update_body_configurations(qstar);

  // compute new pairwise distance information
  _sim->calc_pairwise_distances(); 

  // compute s*
  double sstar = compute_s(pdi);


  // TODO: Add gradient term condition
  while (sstar > s0 + ALPHA * t *(dq))
  {
    // update t
    t *= BETA;

    // update q
    qstar = dq;
    qstar *= t;
    qstar += q;

    // update body configurations
    update_body_configuration(qstar);

    // compute new pairwise distance information
    _sim->calc_pairwise_distances();

    // compute new s*
    sstar = compute_s(pdi);
  }

  // all done? update q
  q = qstar;
}

/// Computes s based on current pairwise distance info
double ConstraintStabilization::compute_s(const vector<PairwiseDistInfo>& pdi)
{
  double s = std::max(get_min_pairwise_dist(pdi), 0.0);

  std::vector<DynamicBodyPtr> bodies = _sim->_bodies;

  // iterate through all joints and check for violated limits
  for (unsigned i = 0; i < bodies.size(); i++)
  {
    ArticulatedBodyPtr art;
    if(art = dynamic_pointer_cast<ArticulatedBodyPtr>(bodies[i]))
    {
      std::vector<JointPtr> joints = art->get_joints();
      for (unsigned j = 0 ; j < joints.size(); j++)
      {
        for (unsigned k = 0 ; k < joints[i]->num_dof(); k++)
        {
          double q = joints[i]->q[j];

          // find the largest violation
          s = std::max(q - joints[i]->hilimit[j], joints[i]->lolimit[j] - q , s)
        }
      }
    }

  //if violated, return amount violated

  return s;
}

/// Gets the body configurations, placing them into q 
void ConstraintStabilization::get_body_configurations(VectorNd& q)
{  

  std::vector<DynamicBodyPtr> bodies = _sim->_bodies;

  q = new VectorNd();

  BOOST_FOREACH(DynamicBodyPtr* body, bodies){
    Ravelin::VectorNd nextVector = (*body)->get_generalized_coordinates(DynamicBody::eEuler, nextVector);
    VectorNd::concat(q,nextVector,q);
  }
}

void ConstraintStabilization::generate_body_index_map(std::map<DynamicBodyPtr, unsigned>& body_index_map)
{
  std::vector<DynamicBodyPtr> bodies = _sim->_bodies;
  unsigned curIndex = 0;

  BOOST_FOREACH(DynamicBodyPtr* body, bodies){

    body_index_map.insert(std::Pair<DynamicBodyPtr, unsigned>(*body, curIndex));
    curIndex += *(body)->num_generalized_coordinates(DynamicBody::eEuler);
    
  }
}

/// Updates the body configurations given q
void ConstraintStabilization::update_body_configurations(const VectorNd& q)
{

  std::vector<DynamicBodyPtr> bodies = _sim->_bodies;
  const unsigned start = 0;
  BOOST_FOREACH(DynamicBodyPtr* body, bodies){
    const unsigned ngc = body->num_generalized_coordinates(DynamicBody::eEuler);
    Ravelin::SharedVectorNd gc_shared = q.segment(last,ngc);
    body->set_generalized_coordinates(DynamicBody::eEuler, gc_shared);
    last = ngc;
  }


}


