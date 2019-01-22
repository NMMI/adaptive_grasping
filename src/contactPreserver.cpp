#include "contactPreserver.h"
#include "ros/ros.h"
#include "utils/pseudo_inversion.h"

#define EXEC_NAMESPACE    "adaptive_grasping"
#define CLASS_NAMESPACE   "contact_preserver"
#define DEBUG             0   // print out additional info

/**
* @brief The following are functions of the class contactPreserver.
*
*/

using namespace adaptive_grasping;

/* DEFAULT CONSTRUCTOR */
contactPreserver::contactPreserver(){
  // Nothing to do here
}

/* CONSTRUCTOR */
contactPreserver::contactPreserver(Eigen::MatrixXd S_){
  // Initializing the object
  initialized = initialize(S_);
}

/* DESTRUCTOR */
contactPreserver::~contactPreserver(){
  // Nothing to do
}

/* INITIALIZE */
bool contactPreserver::initialize(Eigen::MatrixXd S_){
  // Set the synergy matrix
  changeHandType(S_);

  // Setting temporary values of x_d and x_d_old
  x_d_old = Eigen::VectorXd::Ones(S_.cols() + 6 + 6);

}

/* CHANGEHANDTYPE */
void contactPreserver::changeHandType(Eigen::MatrixXd S_){
  // Set the new synergy matrix
  S = S_;
  ROS_DEBUG_STREAM("Changed the Synergy Matrix inside contact preserver!!!");
}

/* SETGRASPSTATE */
void contactPreserver::setGraspState(Eigen::MatrixXd J_, Eigen::MatrixXd G_,
  Eigen::MatrixXd T_, Eigen::MatrixXd H_){
    // Set the new J, G, T and H matrices
    J = J_; G = G_; T = T_; H = H_;
}

/* SETMINIMIZATIONPARAMS */
void contactPreserver::setMinimizationParams(Eigen::VectorXd x_d_,
  Eigen::MatrixXd A_tilde_){
  // Set the new desired motion vector and weight matrix
  x_d = x_d_; A_tilde = A_tilde_;
}

/* SETPERMUTATIONMATRIX */
void contactPreserver::setPermutationParams(Eigen::MatrixXd P_, int num_contacts_){
  // Setting the permutation matrix
  P = P_;
  num_contacts = num_contacts_;
}

/* SETRMATRIX */
bool contactPreserver::setRMatrix(){
  // Cannot relax more than the rows of Q_tilde
  if(relaxation_order > Q_tilde.rows()) return false;

  // Build R case by case
  if(relaxation_order == 0){
    R = Eigen::MatrixXd::Zero(1, Q_tilde.rows());
  } else if(relaxation_order < x_d.size()) {
    R = Eigen::MatrixXd::Identity(relaxation_order, Q_tilde.rows());
  } else {
    int residual = relaxation_order - x_d.size();
    R = Eigen::MatrixXd::Zero(x_d.size() + num_contacts * residual, x_d.size() + P.cols());
  }

  // Compute R_bar as N(R) transpose
  Eigen::FullPivLU<Eigen::MatrixXd> lu(R);
  R_bar = lu.kernel().transpose();
}

/* PERFORMMINIMIZATION */
Eigen::VectorXd contactPreserver::performMinimization(){
  // Resize Q to be of correct size
  Q.resize(H.rows(), x_d.size());

  // Print message for debug
  if(DEBUG) std::cout << "Resized Q in contactPreserver!" << std::endl;

  // Now create the block matrix
  Eigen::MatrixXd NullMatrix = Eigen::MatrixXd::Zero(H.rows(), G.rows());
  Q << H*J*S, H*T, NullMatrix-H*G.transpose();

  // Print message for debug
  if(DEBUG) std::cout << "Computed Q in contactPreserver!" << std::endl;

  // Now create Q_tilde by appending Q under Identity matrix
  Q_tilde.resize(x_d.size() + H.rows(), x_d.size());
  Q_tilde << Eigen::MatrixXd::Identity(x_d.size(), x_d.size()), Q;

  // Print message for debug
  if(DEBUG) std::cout << "Computed Q_tilde in contactPreserver!" << std::endl;

  // Compute vector y
  y.resize(x_d.size() + H.rows());
  y << x_d, Eigen::VectorXd::Zero(H.rows());

  // If the desired motion has changed, reset R and R_bar
  if(!(x_d - x_d_old).isMuchSmallerThan(0.0001) || first_it){
    relaxation_order = 0;               // Nothing is to be relaxed
    if(first_it) first_it = false;
  }

  x_d_old = x_d;

  // Check the first condition of algorithm
  pseudo_inverse(R_bar * Q_tilde, pinv_R_bar_Q_tilde, false);             // Undamped pseudo inversion of (R_bar * Q_tilde)
  if((R_bar * Q_tilde * pinv_R_bar_Q_tilde * R_bar * y - R_bar * y).isMuchSmallerThan(0.0001)){
    // Compute a basis of the null space of Q_tilde by using LU decomposition
    Eigen::FullPivLU<Eigen::MatrixXd> lu(Q_tilde);
    N_tilde = lu.kernel();
    ROS_DEBUG_STREAM("N_tilde(Q) = \n" << N_tilde << ".");

    // Print message for debug
    if(DEBUG) std::cout << "Computed N_tilde(Q) in contactPreserver!" << std::endl;

    // Computing the solution
    C = N_tilde.transpose() * Q_tilde.transpose() * R.transpose() * 

  }


  // Finally, compute the reference motion that preserves the contacts
  Eigen::MatrixXd InverseBlock = (N.transpose()*A_tilde*N).inverse();
  if(DEBUG) std::cout << "Lambda = " << std::endl; 
  if(DEBUG) std::cout << InverseBlock*N.transpose()*A_tilde*x_d << "." << std::endl;
  Eigen::VectorXd x_ref = N*InverseBlock*N.transpose()*A_tilde*x_d;

  // // If the value of the synergy is almost null, perform the relaxation
  // if(std::abs(x_ref(1)) < 0.0001){
  //   ROS_WARN_STREAM("contactPreserver::performMinimization The synergy value is small: relaxing the constraints!.");

  //   // Partitioning of Q using the permutation matrix P
  //   if(true) std::cout << "The NON permuted Q = \n" << Q << "." << "\n-----------\n" << std::endl;
  //   Q = P * Q;
  //   if(DEBUG) std::cout << "Computed the permuted Q = \n" << Q << "." << std::endl;

  //   // Computing the size of Q_2
  //   size_Q_2 = Q.rows() - size_Q_1;
  //   if(DEBUG) std::cout << "Computed the size of Q_2 which is " << size_Q_2 << "." << std::endl;

  //   // Defining Q_1 and Q_2 and computing the null of Q_1
  //   Q_1 = Q.block(0, 0, size_Q_1, Q.cols());
  //   if(true) std::cout << "Computed Q_1: \n" << Q_1 << "\n-----------\n" << std::endl;
  //   Q_2 = Q.block(size_Q_1, 0, size_Q_2, Q.cols());
  //   if(true) std::cout << "Computed Q_2: \n" << Q_2 << "\n-----------\n" << std::endl;

  //   // Compute a basis of the null space by using LU decomposition
  //   Eigen::FullPivLU<Eigen::MatrixXd> lu_1(Q_1);
  //   N = lu_1.kernel();                                    // ATTENTION: overwriting the previous null matrix
  //   ROS_DEBUG_STREAM("N_1(Q_1) = \n" << N << ".");
  //   if(true) std::cout << "Computed N_1(Q_1) in contactPreserver: \n" << N << "." << std::endl;

  //   // Finally, compute the reference motion that preserves the contacts
  //   InverseBlock = (N.transpose() * (A_tilde + Q_2.transpose() * Q_2) * N).inverse();
  //   if(DEBUG) std::cout << "Lambda Relaxed = " << std::endl; 
  //   if(DEBUG) std::cout << InverseBlock*N.transpose()*A_tilde*x_d << "." << std::endl;
  //   x_ref = N*InverseBlock*N.transpose()*A_tilde*x_d;

  //   ROS_WARN_STREAM("contactPreserver::performMinimization Finished relaxing the constraints!.");
  // }

  // Return contact preserving solution
  return x_ref;
  
  // For debugging purpouses (real line is above)
  // Eigen::VectorXd null_vec_debug = N.col(0);
  // return null_vec_debug;
}

/* PRINTALL */
void contactPreserver::printAll(){
  // Print to screen the main private variables
  std::cout << "J =" << std::endl; std::cout << J << std::endl;
  std::cout << "G =" << std::endl; std::cout << G << std::endl;
  std::cout << "T =" << std::endl; std::cout << T << std::endl;
  std::cout << "H =" << std::endl; std::cout << H << std::endl;
  std::cout << "S =" << std::endl; std::cout << S << std::endl;
  std::cout << "A_tilde =" << std::endl; std::cout << A_tilde << std::endl;
  std::cout << "x_d =" << std::endl; std::cout << x_d << std::endl;
  std::cout << "Q =" << std::endl; std::cout << Q << std::endl;
  std::cout << "N =" << std::endl; std::cout << N << std::endl;
}
