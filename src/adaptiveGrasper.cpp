#include "adaptiveGrasper.h"
#include <eigen_conversions/eigen_msg.h>

#define EXEC_NAMESPACE    "adaptive_grasping"
#define CLASS_NAMESPACE   "adaptive_grasper"

#define DEBUG   0           // Prints out additional info (additional to ROS_DEBUG)
#define DEBUG_PUB         1   // publishes additional info for rqt_plot

/**
* @brief The following are functions of the class adaptiveGrasper.
*
*/

using namespace adaptive_grasping;

/* CONSTRUCTOR */
adaptiveGrasper::adaptiveGrasper(){
    // Nothing to do here
}

/* OVERLOADED CONSTRUCTOR */
adaptiveGrasper::adaptiveGrasper(std::vector<std::string> param_names){
    // Building the object
    this->initialized = this->initialize(param_names);
}

/* DESTRUCTOR */
adaptiveGrasper::~adaptiveGrasper(){
    // Nothing to do here now
}

/* INITIALIZE */
bool adaptiveGrasper::initialize(std::vector<std::string> param_names){
    // Subscribe to joint states
    this->js_sub = this->ag_nh.subscribe("joint_states", 1, &adaptiveGrasper::getJointsAndComputeSyn, this);
    ROS_INFO_STREAM("adaptiveGrasper::initialize A SUBSCRIBER SUBSCRIBED TO " << js_sub.getTopic() << ".");

    // Getting ready to run the algo
    this->run = false;

    // Waiting for a message in joint states
    this->full_joint_state = ros::topic::waitForMessage<sensor_msgs::JointState>("/joint_states", this->ag_nh);

    // Initializing the client to robot commander
    this->client_rc = this->ag_nh.serviceClient<adaptive_grasping::velCommand>("rc_service");

    // Initializing the server to adaptive grasper
    this->server_ag = this->ag_nh.advertiseService("adaptive_grasper_service", &adaptiveGrasper::agCallback, this);

    // Initializing the client to end adaptive grasping
    this->end_client = this->ag_nh.serviceClient<std_srvs::Trigger>("adaptive_grasping_end_trigger");

    // Starting to parse the needed elements from parameter server
    ROS_INFO_STREAM("adaptiveGrasper::initialize STARTING TO PARSE THE NEEDED VARIABLES!");

    this->initialized = this->ag_nh.getParam("adaptive_grasping", this->adaptive_params);
    if(this->initialized == false){
        ROS_ERROR_STREAM("adaptiveGrasper::initialize could not find the needed params");
    }
    this->initialized = this->parseParams(this->adaptive_params, param_names);

    // Subscribing to panda softhand safety
    this->safety_sub = this->ag_nh.subscribe("/panda_softhand_safety_info", 1, &adaptiveGrasper::getSafetyInfo, this);
    ROS_INFO_STREAM("adaptiveGrasper::initialize A SUBSCRIBER SUBSCRIBED TO " << safety_sub.getTopic() << ".");

    // Waiting for a message in panda softhand safety
    panda_softhand_safety::SafetyInfo::ConstPtr tmp_safety = ros::topic::waitForMessage<panda_softhand_safety::SafetyInfo>("/panda_softhand_safety_info", this->ag_nh);

    // Subscribing to object pose
    this->op_sub = this->ag_nh.subscribe(this->object_topic_name, 1, &adaptiveGrasper::getObjectPose, this);
    ROS_INFO_STREAM("adaptiveGrasper::initialize A SUBSCRIBER SUBSCRIBED TO " << op_sub.getTopic() << ".");

    // Waiting for a message in object pose
    geometry_msgs::Pose::ConstPtr tmp_op = ros::topic::waitForMessage<geometry_msgs::Pose>(this->object_topic_name, this->ag_nh);

    // Building the main objects
    ROS_INFO_STREAM("adaptiveGrasper::initialize STARTING TO BUILD THE OBJECTS!");

    this->my_contact_state.intialize(this->touch_topic_name, this->link_names_map, this->params_map);
    this->my_matrices_creator.initialize(this->H_i, this->params_map.at("world_name"), this->params_map.at("palm_name"), this->joint_numbers);
    this->my_contact_preserver.initialize(this->S);

    // Resetting the reference motion to zero
    this->x_ref = Eigen::VectorXd::Zero(this->x_d.size());

    // Setting up the RViz object marker publisher
    this->marker_pub = ag_nh.advertise<visualization_msgs::Marker>("object_marker", 1);
    this->obj_marker.header.frame_id = "/world";

    // Setting up the publisher for twist
    this->pub_twist_debug = this->ag_nh.advertise<geometry_msgs::WrenchStamped>("/x_ref_debug" , 1);

    ROS_INFO_STREAM("adaptiveGrasper::initialize FINISHED BUILDING THE OBJECTS!");
}

/* PRINTPARSED */
void adaptiveGrasper::printParsed(){
    ROS_INFO_STREAM("\nThe finger touch topic is: " << this->touch_topic_name << ".");
    ROS_INFO_STREAM("\nThe map with link names is:");
    for(auto it : this->link_names_map){
        std::cout << it.first << " : " << it.second << std::endl;
    }
    ROS_INFO_STREAM("\nThe map with params names is:");
    for(auto it : this->params_map){
        std::cout << it.first << " : " << it.second << std::endl;
    }
    ROS_INFO_STREAM("\nThe array of joint finger link no. is:");
    std::cout << "[ ";
    for(auto it : this->joint_numbers){
        std::cout << it << " ";
    }
    std::cout << "]" << std::endl;
    ROS_INFO_STREAM("\nThe contact selection matrix H is: \n" << this->H_i << ".");
    ROS_INFO_STREAM("\nThe contact selection matrix H_2 is: \n" << this->H_i_2 << ".");
    ROS_INFO_STREAM("\nThe min. weight matrix A tilde is: \n" << this->A_tilde << ".");
    ROS_INFO_STREAM("\nThe desired motion x_d is: \n" << this->x_d << ".");
    ROS_INFO_STREAM("\nThe desired motion x_d_2 is: \n" << this->x_d_2 << ".");
    ROS_INFO_STREAM("\nThe object pose topic is: " << this->object_topic_name << ".");
    ROS_INFO_STREAM("\nThe reference scaling factor is: " << this->scaling << ".");
    ROS_INFO_STREAM("\nThe permutation vector is: \n" << this->p_vector << ".");
    ROS_INFO_STREAM("\nThe permutation vector 2 is: \n" << this->p_vector_2 << ".");
    ROS_INFO_STREAM("\nThe touch indexes vector 2 is: \n" << this->touch_indexes << ".");
    ROS_INFO_STREAM("\nThe max synergy threshold is: \n" << this->syn_thresh << ".");
    ROS_INFO_STREAM("\nThe bool relax_to_zero is: \n" << this->relax_to_zero << ".");
    ROS_INFO_STREAM("\nThe bool touch_change is: \n" << this->touch_change << ".");
}

/* PRINTCONTACTSINFO */
void adaptiveGrasper::printContactsInfo(){
    ROS_INFO_STREAM("\nThe map with contacts info is: \n");
    for(auto it : this->read_contacts_map){
        std::cout << it.first << " : \n" << std::get<0>(it.second) << " - " << std::get<1>(it.second).matrix() << " - " << std::get<2>(it.second).matrix() << std::endl;
    }
    ROS_INFO_STREAM("\nThe map with joints info is: \n");
    for(auto it : this->read_joints_map){
        std::cout << it.first << " : " << it.second << std::endl;
    }
}

/* PRINTOBJECTPOSE */
void adaptiveGrasper::printObjectPose(){
    ROS_INFO_STREAM("\nThe object pose is: \n" << this->object_pose.matrix() << ".");
}

/* SPINROS */
void adaptiveGrasper::spinROS(){
    // Starting to spin in order to wait for service calls
    ros::spin();
}

/* PARSEPARAMS */
bool adaptiveGrasper::parseParams(XmlRpc::XmlRpcValue params_xml, std::vector<std::string> param_names){
    // Starting to parse and save all needed single parameters
    parseParameter(params_xml, this->touch_topic_name, param_names[0]);
    parseParameter(params_xml, this->link_names_map, param_names[1]);
    parseParameter(params_xml, this->params_map, param_names[2]);
    parseParameter(params_xml, this->joint_numbers, param_names[3]);
    parseParameter(params_xml, this->H_i, param_names[4]);
    parseParameter(params_xml, this->A_tilde, param_names[5]);

    // x_d (vector) needs to be parsed differently using the parsing function for matrix
    Eigen::MatrixXd temp_x_d;
    parseParameter(params_xml, temp_x_d, param_names[6]);
    this->x_d = temp_x_d.transpose().col(0);

    parseParameter(params_xml, this->spin_rate, param_names[7]);
    parseParameter(params_xml, this->object_topic_name, param_names[8]);
    parseParameter(params_xml, this->scaling, param_names[9]);

    // p_vector (vector) needs to be parsed differently using the parsing function for matrix
    Eigen::MatrixXd temp_p_vector;
    parseParameter(params_xml, temp_p_vector, param_names[10]);
    this->p_vector = temp_p_vector.transpose().col(0);

    parseParameter(params_xml, this->syn_thresh, param_names[11]);
    parseParameter(params_xml, this->relax_to_zero, param_names[12]);
    parseParameter(params_xml, this->touch_change, param_names[13]);

    parseParameter(params_xml, this->H_i_2, param_names[14]);

    // p_vector_2 (vector) needs to be parsed differently using the parsing function for matrix
    Eigen::MatrixXd temp_p_vector_2;
    parseParameter(params_xml, temp_p_vector_2, param_names[15]);
    this->p_vector_2 = temp_p_vector_2.transpose().col(0);

    // x_d_2 (vector) needs to be parsed differently using the parsing function for matrix
    Eigen::MatrixXd temp_x_d_2;
    parseParameter(params_xml, temp_x_d_2, param_names[16]);
    this->x_d_2 = temp_x_d_2.transpose().col(0);

    // touch_indexes (vector) needs to be parsed differently using the parsing function for matrix
    Eigen::MatrixXd temp_touch_indexes;
    parseParameter(params_xml, temp_touch_indexes, param_names[17]);
    this->touch_indexes = temp_touch_indexes.transpose().col(0);
}

/* SETCOMMANDANDSEND */
bool adaptiveGrasper::setCommandAndSend(Eigen::VectorXd ref_vec, adaptive_grasping::velCommand comm){
    // Clearing the previous service file
    comm.request.x_ref.clear();
    
    // Filling up the request
    comm.request.x_ref.push_back(ref_vec(0));
    comm.request.x_ref.push_back(ref_vec(1));
    comm.request.x_ref.push_back(ref_vec(2));
    comm.request.x_ref.push_back(ref_vec(3));
    comm.request.x_ref.push_back(ref_vec(4));
    comm.request.x_ref.push_back(ref_vec(5));
    comm.request.x_ref.push_back(ref_vec(6));

    if(this->client_rc.call(comm)){
      if(DEBUG && false) ROS_INFO_STREAM("adaptiveGrasper::setCommandAndSend Success!");
      return true;
    } else {
      if(DEBUG) ROS_INFO_STREAM("adaptiveGrasper::setCommandAndSend Failed!");
      return false;
    }
}

/* GETJOINTSANDCOMPUTESYN */
void adaptiveGrasper::getJointsAndComputeSyn(const sensor_msgs::JointState::ConstPtr &msg){
    // Storing the message into another global message variable
	ROS_DEBUG_STREAM("adaptiveGrasper::getJointsAndComputeSyn GOT JOINTSTATE MSG: STARTING TO SAVE!");
	this->full_joint_state = msg;
	ROS_DEBUG_STREAM("adaptiveGrasper::getJointsAndComputeSyn SAVED JOINTSTATE MSG!");

    // Using the full joint state to compute the ratios
    Eigen::MatrixXd Syn(33, 1);

    // Copying the values of the joints
    int index = find (this->full_joint_state->name.begin(),this->full_joint_state->name.end(), "right_hand_thumb_abd_joint") - this->full_joint_state->name.begin();
    if(DEBUG && false) std::cout << "********************************* " << std::endl;
    for(int j = 0; j < 33; j++){
        if(DEBUG && false) std::cout << "Getting the joint " << this->full_joint_state->name[index + j] << " with value " << this->full_joint_state->position[index + j] << "." << std::endl;
        Syn(j) = this->full_joint_state->position[index + j];
    }
    if(DEBUG && false) std::cout << "********************************* " << std::endl;

    // Dividing by synergy value to find the Matrix
    this->adaptive_grasper_mutex.lock();
    this->S = Syn / this->full_joint_state->position[index - 1];
    this->adaptive_grasper_mutex.unlock();

    // Checking if the synergy value is over a threshold and setting run bool accordingly (for stopping the grasping)
    if(this->run && (this->full_joint_state->position[index - 1] > this->syn_thresh)){
        this->adaptive_grasper_mutex.lock();
        this->run = false;
        this->adaptive_grasper_mutex.unlock();
        // Resetting the contact state
        this->my_contact_state.resetContact();       // Might cause crashing

        // Calling the end adaptive grasping service to let full grasper know that it ended
        if(this->end_client.call(this->end_srv)){
            ROS_WARN_STREAM("adaptiveGrasper : Triggered stop adaptive grasping!");
        } else {
            ROS_ERROR_STREAM("adaptiveGrasper : something went wrong trying to trigger the stop..!");
        }

        ROS_INFO_STREAM("adaptiveGrasper : The hand is almost fully closed: stopping the grasping!");
    }

}

/* GETSAFETYINFO */
void adaptiveGrasper::getSafetyInfo(const panda_softhand_safety::SafetyInfo::ConstPtr &msg){
    // Checking if the collision is going to happen and setting run bool accordingly (for stopping the grasping)
    if( this->run && ((msg->collision) || (msg->joint_position_limits) || (msg->joint_velocity_limits)) ){
        this->adaptive_grasper_mutex.lock();
        this->run = false;
        this->adaptive_grasper_mutex.unlock();
        // Resetting the contact state
        this->my_contact_state.resetContact();       // Might cause crashing

        // Calling the end adaptive grasping service to let full grasper know that it ended
        if(this->end_client.call(this->end_srv)){
            ROS_WARN_STREAM("adaptiveGrasper : Triggered stop adaptive grasping!");
        } else {
            ROS_ERROR_STREAM("adaptiveGrasper : something went wrong trying to trigger the stop..!");
        }

        if(msg->collision) ROS_INFO_STREAM("adaptiveGrasper : The robot is about to collide: stopping the grasping!");
        else ROS_INFO_STREAM("adaptiveGrasper : The robot is about to violate joint limits: stopping the grasping!");
    }
}

/* GETOBJECTPOSE */
void adaptiveGrasper::getObjectPose(const geometry_msgs::Pose::ConstPtr &msg){
    // Saving to the eigen affine
    this->adaptive_grasper_mutex.lock();
    tf::poseMsgToEigen(*msg, this->object_pose);
    this->adaptive_grasper_mutex.unlock();

    // Publishing the object to RViz
    this->obj_marker.header.stamp = ros::Time::now();
    this->obj_marker.ns = "adaptive_grasping";
    this->obj_marker.id = 0;
    this->obj_marker.type = shape;
    this->obj_marker.action = visualization_msgs::Marker::ADD;
    this->obj_marker.pose.position.x = this->object_pose.translation()[0]; this->obj_marker.pose.position.y = this->object_pose.translation()[1];
    this->obj_marker.pose.position.z = this->object_pose.translation()[2];
    this->obj_marker.scale.x = 0.05; this->obj_marker.scale.y = 0.05; this->obj_marker.scale.z = 0.05;
    this->obj_marker.color.r = 0.0f; this->obj_marker.color.g = 1.0f; this->obj_marker.color.b = 0.0f; this->obj_marker.color.a = 1.0f;
    this->obj_marker.lifetime = ros::Duration(0);
    this->marker_pub.publish(this->obj_marker);
}

/* AGCALLBACK */
bool adaptiveGrasper::agCallback(adaptive_grasping::adaptiveGrasp::Request &req, adaptive_grasping::adaptiveGrasp::Response &res){
    if(DEBUG) ROS_INFO_STREAM("Entering the AG callback!");

    // Checking if request is true and return otherwise
    if(!req.run_adaptive_grasp){
        if(DEBUG) ROS_INFO_STREAM("The request run adaptive grasp is FALSE!");
        this->adaptive_grasper_mutex.lock();
        this->run = false;
        this->adaptive_grasper_mutex.unlock();
        // Resetting the contact state
        this->my_contact_state.resetContact();
        res.success = false;

        // Calling the end adaptive grasping service to let full grasper know that it ended
        if(this->end_client.call(this->end_srv)){
            ROS_WARN_STREAM("adaptiveGrasper : Triggered stop adaptive grasping!");
        } else {
            ROS_ERROR_STREAM("adaptiveGrasper : something went wrong trying to trigger the stop..!");
        }

        return true;
    }

    if(DEBUG) ROS_INFO_STREAM("The request run adaptive grasp is TRUE!");
    // Resetting the contact state
    this->my_contact_state.resetContact();
    // Setting the run to true
    this->adaptive_grasper_mutex.lock();
    this->run = true;
    this->adaptive_grasper_mutex.unlock();
    res.success = true;
    return true;
}

/* SPINGRASPER */
void adaptiveGrasper::spinGrasper(){
    // Setting the ROS rate
    ros::Rate rate(this->spin_rate);

    // (TODO: might need to start an AsyncSpinner here.)

    // Starting the ROS loop
    while(ros::ok()){
        // Spinning once to process callbacks
        ros::spinOnce();

        if(this->run){
            // Reading the values from contact state
            this->my_contact_state.readValues(this->read_contacts_map, this->read_joints_map);
            this->contacts_num = this->read_contacts_map.size();

            // Printing contacts info and synergy matrix
            if(DEBUG) ROS_INFO_STREAM("\nSynergy Matrix S: \n" << this->S << ".\n");
            if(DEBUG) this->printContactsInfo();
            if(DEBUG) this->printObjectPose();

            // Setting the necessary things in matrix creator and computing matrices
            this->my_matrices_creator.setContactsMap(this->read_contacts_map);
            this->my_matrices_creator.setJointsMap(this->read_joints_map);
            this->my_matrices_creator.setObjectPose(this->object_pose);
            
            // Single contact -> fully constrained / Multiple contacts -> position constrained
            if(this->read_contacts_map.size() > 1 && this->touch_change){
                this->my_matrices_creator.changeContactType(this->H_i_2);
                this->my_matrices_creator.setPermutationVector(this->p_vector_2);
            } else {
                this->my_matrices_creator.changeContactType(this->H_i);
                this->my_matrices_creator.setPermutationVector(this->p_vector);
            }

            // Setting the other stuff for whole permutation computation in matricesCreator
            this->my_matrices_creator.setOtherPermutationStuff(this->touch_indexes);

            // Computing all matrices
            this->my_matrices_creator.computeAllMatrices();

            // Reading and couting the matrices
            this->my_matrices_creator.readAllMatrices(this->read_J, this->read_G, this->read_T, this->read_H, this->read_P);
            if(DEBUG){
                ROS_INFO_STREAM("adaptiveGrasper::spinGrasper The created matrices are: ");
                ROS_INFO_STREAM("\nJ = " << "\n" << this->read_J << "\n");
                ROS_INFO_STREAM("\nG = " << "\n" << this->read_G << "\n");
                ROS_INFO_STREAM("\nT = " << "\n" << this->read_T << "\n");
                ROS_INFO_STREAM("\nH = " << "\n" << this->read_H << "\n");
                ROS_INFO_STREAM("\nP = " << "\n" << this->read_P << "\n");
            }
            // Printing out the contacts map
            if(this->read_contacts_map.size() > 0 && DEBUG){
              std::cout << "Current contacts are:" << std::endl;
              for(auto elem : this->read_contacts_map){
                std::cout << elem.first << " : " << std::get<0>(elem.second) << "." << std::endl;
              }
            }

            // Setting the synergy matrix in preserver
            this->my_contact_preserver.changeHandType(this->S);

            // Setting the reference motion to the desired one (Single/Multiple)
            if(this->read_contacts_map.size() > 1 && this->touch_change){
                this->x_ref = this->x_d_2;
                if(DEBUG) ROS_INFO_STREAM("adaptiveGrasper::spinGrasper Two contacts, changing x_d!");
            } else {
                this->x_ref = this->x_d;
            }

            // Performing the minimization only if there are contacts (i.e. the matrices are not empty)
            if(read_J.innerSize() > 0 && read_G.innerSize() > 0 && read_T.innerSize() > 0 && read_H.innerSize() > 0){
                // Setting grasp state
                this->my_contact_preserver.setGraspState(this->read_J, this->read_G, this->read_T, this->read_H);

                // Setting minimization and relaxation parameters
                if(this->read_contacts_map.size() > 1 && this->touch_change){
                    this->my_contact_preserver.setMinimizationParams(this->x_d_2, this->A_tilde);
                    if(DEBUG) ROS_WARN_STREAM("adaptiveGrasper::spinGrasper Two contacts, changing x_d and A_tilde!");
                } else {
                    this->my_contact_preserver.setMinimizationParams(this->x_d, this->A_tilde);
                }
                this->my_contact_preserver.setPermutationParams(this->read_P, this->contacts_num);

                // Performing minimization
                bool no_relaxation = this->my_contact_preserver.performMinimization(this->x_ref);

                // Publish the twist for debug
                if (DEBUG_PUB){
                    // Filling and publishing the twist for debug
                    this->twist_wrench.header.frame_id = "world";
                    this->twist_wrench.header.stamp = ros::Time::now();
                    this->twist_wrench.wrench.force.x = this->x_ref(1); this->twist_wrench.wrench.torque.x = this->x_ref(4);
                    this->twist_wrench.wrench.force.y = this->x_ref(2); this->twist_wrench.wrench.torque.y = this->x_ref(5);
                    this->twist_wrench.wrench.force.z = this->x_ref(3); this->twist_wrench.wrench.torque.z = this->x_ref(6);
                    this->pub_twist_debug.publish(this->twist_wrench);
                }

                // If needed set reference to null twist
                if(!no_relaxation && this->relax_to_zero){
                    this->x_ref = Eigen::VectorXd::Zero(this->x_ref.size());
                }

                if(DEBUG) ROS_DEBUG_STREAM("adaptiveGrasper::spinGrasper Performed Minimization!!!");
            }

            // Scaling the reference and sending to the robot commander
            this->x_ref = this->scaling * this->x_ref;
            if(DEBUG) ROS_INFO_STREAM("The reference to be sent to the commander is: \n" << this->x_ref << ".");

            if(!this->setCommandAndSend(this->x_ref, this->ref_command)){
                ROS_ERROR_STREAM("adaptiveGrasper::spinGrasper Something went wrong while sending the reference to the commander while sending x_ref!");
            }
        } else {
            // If the run bool is false publish zero twist and speed
            zero_ref = Eigen::VectorXd::Zero(this->x_ref.size());
            if(!this->setCommandAndSend(zero_ref, this->ref_command)){
                ROS_ERROR_STREAM("adaptiveGrasper::spinGrasper Something went wrong while sending the reference to the commander while sending zeros!");
            }
        }

        // Rate
        rate.sleep();
    }


    // Finished adaptive grasping, returning
    ROS_INFO_STREAM("Finished Adaptive Grasping: returning!!!");
}
