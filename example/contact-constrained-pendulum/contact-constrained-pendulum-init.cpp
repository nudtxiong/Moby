/*****************************************************************************
 * "Controller" for constrained pendulum example 
 ****************************************************************************/
#include <Moby/ConstraintSimulator.h>
#include <Moby/RCArticulatedBody.h>
#include <Moby/GravityForce.h>
#include <Ravelin/Pose3d.h>
#include <Ravelin/Vector3d.h>
#include <Ravelin/VectorNd.h>
#include <fstream>
#include <stdlib.h>

using boost::shared_ptr;
using namespace Ravelin;
using namespace Moby;

Moby::RigidBodyPtr l1;
boost::shared_ptr<ConstraintSimulator> sim;
boost::shared_ptr<GravityForce> grav;

// setup simulator callback
void post_step_callback(Simulator* sim)
{
  const unsigned Y = 1, Z = 2;

  // output the energy of the link
  std::ofstream out("energy.dat", std::ostream::app);
  Transform3d gTw = Pose3d::calc_relative_pose(l1->get_pose(), GLOBAL);
  double KE = l1->calc_kinetic_energy();
  double PE = l1->get_inertia().m*(gTw.x[Y]+1.0)*-grav->gravity[Y];
  out << KE << " " << PE << " " << (KE+PE) << std::endl;
  out.close();
  SVelocityd v = l1->get_velocity();
}

/// plugin must be "extern C"
extern "C" {

void init(void* separator, const std::map<std::string, Moby::BasePtr>& read_map, double time)
{
  const unsigned Z = 2;

  // kill the existing files
  std::ofstream out("energy.dat");
  out.close();
  out.open("cvio.dat");
  out.close();

  // get a reference to the ConstraintSimulator instance
  for (std::map<std::string, Moby::BasePtr>::const_iterator i = read_map.begin();
       i !=read_map.end(); i++)
  {
    // Find the simulator reference
    if (!sim)
      sim = boost::dynamic_pointer_cast<ConstraintSimulator>(i->second);
    if (i->first == "l1")
      l1 = boost::dynamic_pointer_cast<RigidBody>(i->second);
    if (!grav)
      grav = boost::dynamic_pointer_cast<GravityForce>(i->second);
  }

  sim->post_step_callback_fn = &post_step_callback;
}
} // end extern C
