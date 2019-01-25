#ifndef CONTACT_PRESERVER_H
#define CONTACT_PRESERVER_H

#include <iostream>
#include <Eigen/Dense>

/**
* @brief This class is called by the adaptive_grasping method to compute the
* contact preserving reference motions.
*
*/

namespace adaptive_grasping {

  class contactPreserver {

  public:

    /** DEFAULT CONSTRUCTOR
    * @brief Default constructor for contactPreserver
    *
    * @param null
    * @return null
    */
    contactPreserver();

    /** CONSTRUCTOR
    * @brief Overloaded constructor for contactPreserver
    *
    * @param S_
    *   the synergy matrix of the hand (if hand fully actuated S = I)
    * @return null
    */
    contactPreserver(Eigen::MatrixXd S_);

    /** DESTRUCTOR
    * @brief Default destructor for contactPreserver
    *
    * @param null
    * @return null
    */
    ~contactPreserver();

    // A boolean for checking if the object has been initialized
    bool initialized = false;

    /** INITIALIZE
    * @brief Private function to initialize the object
    *
    * @param S_
    *   the synergy matrix of the hand (if hand fully actuated S = I)
    * @return null
    */
    bool initialize(Eigen::MatrixXd S_);

    /** CHANGEHANDTYPE
    * @brief Function to eventually change the hand type (set new S)
    *
    * @param S_
    *   the new synergy matrix
    * @return null
    */
    void changeHandType(Eigen::MatrixXd S_);

    /** SETGRASPSTATE
    * @brief Function to set new values for J, G, T and H
    *
    * @param J_
    *   the whole jacobian for the contacts
    * @param G_
    *   the whole grasp matrix for the contacts
    * @param T_
    *   the whole palm twist pole change matrix for the contacts
    * @param H_
    *   the new contact selection matrix
    * @return null
    */
    void setGraspState(Eigen::MatrixXd J_, Eigen::MatrixXd G_, Eigen::MatrixXd T_,
      Eigen::MatrixXd H_);

    /** SETMINIMIZATIONPARAMS
    * @brief Function to set new values for the minimization problem
    *
    * @param x_d_
    *   the desired motion of the hand given by some high level planner
    * @param A_tilde_
    *   the weight matrix in the cost function of the minimization
    * @return null
    */
    void setMinimizationParams(Eigen::VectorXd x_d_, Eigen::MatrixXd A_tilde_);

    /** SETPERMUTATIONPARAMS
    * @brief Function to set new permutation matrix for the relaxed minimization problem (used to compute R)
    *
    * @param P_
    *   the whole permutation matrix
    * @param num_contacts_
    *   the number of fingers in contact
    * @return null
    */
    void setPermutationParams(Eigen::MatrixXd P_, int num_contacts_);

    /** SETRMATRIX
    * @brief Function to build relaxation matrices R and R_bar from order of relaxation
    *
    * @return bool
    */
    bool setRMatrix();

    /** UPDATEAMATRIX
    * @brief Function to update A_tilde according to R
    *
    * @return void
    */
    void updateAMatrix();

    /** PERFORMMINIMIZATION
    * @brief Function to perform the minimization using current values
    *
    * @return Eigen::VectorXd x_ref the resulting motion that preserves contacts
    */
    Eigen::VectorXd performMinimization();

    /** PRINTALL
    * @brief Function to print out to console all relevant variables
    *
    * @return null
    */
    void printAll();

  private:
    // Boolean for checking if first iteration
    bool first_it = true;

    // Current contacts jacobian
    Eigen::MatrixXd J;

    // Grasp matrix
    Eigen::MatrixXd G;

    // Pole change matrix (twist of palm to contacts)
    Eigen::MatrixXd T;

    // Contact selection Matrix
    Eigen::MatrixXd H;

    // Synergy Matrix
    Eigen::MatrixXd S;

    // Permutation Matrix
    Eigen::MatrixXd P;

    // Planner desired motions
    Eigen::VectorXd x_d;

    // Previous planner desired motions
    Eigen::VectorXd x_d_old;

    // Particular solution x_star
    Eigen::VectorXd x_star;

    // Full minimization solution
    Eigen::VectorXd x_ref;

    // Constant term for Q_tilde (obtained by appending zeros under x_d)
    Eigen::VectorXd y;

    // Minimization weights parsed
    Eigen::MatrixXd A_tilde_parsed;

    // Minimization weights current, complies with R
    Eigen::MatrixXd A_tilde;

    // Contact relation matrix
    Eigen::MatrixXd Q;

    // Old contact relation matrix
    Eigen::MatrixXd Q_old;

    // Q_tilde matrix
    Eigen::MatrixXd Q_tilde;

    // Relaxation matrix R
    Eigen::MatrixXd R;

    // Relaxation matrix R_bar
    Eigen::MatrixXd R_bar;

    // Pseudo inverse of R_bar Q_tilde
    Eigen::MatrixXd pinv_R_bar_Q_tilde;

    // The C matrix in the algorithm
    Eigen::MatrixXd C;

    // Number of times the relax functon has been called for the same x_d
    int relaxation_order;

    // Number of contacting fingers
    int num_contacts;

    // Null space basis of Q_tilde
    Eigen::MatrixXd N_tilde;

  }; // closing class

} // closing namespace



#endif // CONTACT_PRESERVER_H
