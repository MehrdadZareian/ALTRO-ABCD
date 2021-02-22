#include <fstream>
#include <iostream>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <unordered_map>


/* SCOTS header */
#include "scots.hh"
/* ode solver */
#include "RungeKutta4.hh"
/* time profiling */
#include "TicToc.hh"
/* memory profiling */
#include <ctime>
#include <sys/resource.h>
#include "formation.hh"

#define SIMULATION false


#define pi 3.14



#define obstacle_size 1
struct rusage usage;




bool check_inside(state_type left,state_type right, state_type point ){
    for (int i = 0; i < state_dim; ++i) {
        if(point[i] < left[i] || point[i] > right[i])
            return false;
    }
    return true;
}





/* In this function we read control inputs from a file and simulate the trajectroy using inputs + ODE */

std::vector<state_type> trajectory_simulation(state_type initial_trajectory_state ){
    std::vector<state_type> nominal_trajectory_states;

    int trajectory_size=parameters.trajectory_size;

    std::ifstream inFile;
    inFile.open(parameters.address);


    std::vector<input_type> u_temp(6);
    std::vector<input_type> inputs(trajectory_size);

    for ( int i = 0; i <trajectory_size ; i++) {
        for (int k = 0; k < 6; ++k) {
            inFile >> u_temp[k][0] >>u_temp[k][1];
        }
        inputs[i]=u_temp[4];
    }

    state_type temp_state;
    nominal_trajectory_states.push_back(initial_trajectory_state);
    temp_state=initial_trajectory_state;
    state_type min_x=initial_trajectory_state;
    state_type max_x=initial_trajectory_state;


    for ( int i = 0; i <trajectory_size-1 ; i++)
    {
        ode_post (temp_state,inputs[i]);
        if(temp_state[0]>15){
            trajectory_size=i+1;
            parameters.trajectory_size=i+1;
            break;
        }

        for (int i = 0; i < state_dim; ++i) {
            if(min_x[i]>temp_state[i])
                min_x[i]=temp_state[i];
            if(max_x[i]<temp_state[i])
                max_x[i]=temp_state[i];
        }

        nominal_trajectory_states.push_back(temp_state);
        if(verbose)
            std::cout<< i+2<<"- " << nominal_trajectory_states[i+1][0]<< "  " <<nominal_trajectory_states[i+1][1]<<"  " <<nominal_trajectory_states[i+1][2]<<"  " << std::endl;
    }
    if(verbose)
        for (int i = 0; i < state_dim; ++i)
            std::cout<<i<<" min:"<<min_x[i]<<" max:"<<max_x[i]<<std::endl;

    return nominal_trajectory_states;

}






int main(){


    

    /* to measure time */
    TicToc tt;
    TicToc tt2;
    

    /* setup the workspace of the synthesis problem and the uniform grid */
    /* lower bounds of the hyper rectangle */


    state_type initial_trajectory_state=parameters.initial_trajectory_states[parameters.trajectory_num];
    //__________________________________________________________________________________________________

    /*initial point for trajectory*/
    //     initial_trajectory_state={{6.0,1.2,0,0}};//1
    //    initial_trajectory_state={{3.0,1.2,0,0}};//2
    //    initial_trajectory_state={{0.0,1.2,0,0}};//3
    //   initial_trajectory_state={{8,3.9,-0.64,0}};//4
    //    initial_trajectory_state={{6,5.4,-0.64,0}};//5
    //    initial_trajectory_state={{4.0,6.9,-0.64,0}};//6

    /*vector that contains all of states of nominal trajectory*/


    /* reading nominal controller from a file and simulating   */
    std::vector<state_type> nominal_trajectory_states=trajectory_simulation(parameters);
    int trajectory_size=parameters.trajectory_size;

    /*Target set lower left and upper right*/
    state_type target_left;
    state_type target_right;

    /*specifying target set with lower left and upper right point (last point of trajectory +- radius */
    for (int i = 0; i < state_dim; ++i) {
        double target_radius;
        if(i!=state_dim-1)
            target_radius=parameters.epsilon_num[i]*parameters.s_eta[i];
        else
            target_radius=parameters.s_eta[i]/1e10;
        target_left[i]=nominal_trajectory_states[trajectory_size-1][i]-target_radius;
        target_right[i]=nominal_trajectory_states[trajectory_size-1][i]+target_radius;
    }

    /*uniform grid for state space*/
    scots::UniformGrid ss(state_dim,parameters.s_lb,parameters.s_ub,parameters.s_eta);
    /*uniform grid for input space*/
    scots::UniformGrid is(input_dim,parameters.i_lb,parameters.i_ub,parameters.i_eta);

    /*this print details for uniform grids*/
    if(verbose){
        std::cout << "Uniform grid details:" << std::endl;
        ss.print_info();
        is.print_info();
    }

    /*size of Global state space uniform grid */
    long long unsigned int N=ss.size();
    /*this array stores whether a point is inside of tube or not*/
    bool* inside_of_area= new bool[N];
    /*mapping from global index to local index*/
    abs_type* GtoL=new abs_type[N];
    /*mapping from local index to global index*/
    std::vector<abs_type>  LtoG;


    /* transition function of symbolic model */
    scots::TransitionFunction tf;
    scots::Abstraction<state_type,input_type,Parameters> abs(ss,is);

    //tt2.tic();

    /*This function iterates through all of points inside of the tube and store the indexes of them for building map between global index and local index */
    abs.map_initialization(nominal_trajectory_states,ss,parameters,target_left,target_right,inside_of_area,LtoG,GtoL);
    //tt2.toc();


    std::cout << "Computing the transition function: " << std::endl;
    /* transition function of symbolic model */
    

    /* Computing abstraction */
    tt.tic();
    abs.compute_gb2_temp(tf,ode_post, radius_post,inside_of_area,LtoG,GtoL);
    tt.toc();


    if(!getrusage(RUSAGE_SELF, &usage))
        std::cout << "Memory per transition: " << usage.ru_maxrss/(double)tf.get_no_transitions() << std::endl;
    std::cout << "Number of transitions: " << tf.get_no_transitions() << std::endl;
    std::cout << "mem :" << usage.ru_maxrss<< std::endl;

    /* checking that an index belongs to the target set or not*/
    auto target_index = [&ss,&nominal_trajectory_states,&LtoG,&target_left,&target_right](const abs_type& idx) {
        state_type x;
        ss.itox(LtoG[idx],x);
        if(check_inside(target_left,target_right,x))
            return true;
        return false;
    };


    /*Synthesis*/
    std::cout << "\nSynthesis: " << std::endl;
    tt.tic();
    scots::WinningDomain win=scots::solve_reachability_game(tf,target_index);
    tt.toc();
    std::cout << "Winning domain size: " << win.get_size() << std::endl;


    /*writing static controller to a file*/
    std::cout << "\nWrite controller to controller.scs \n";
    if(write_to_file(scots::StaticController(ss,is,std::move(win)),"controller"))
        std::cout << "Done. \n";

    if(SIMULATION){

        scots::StaticController con;
        if(!read_from_file(con,"controller")) {
            std::cout << "Could not read controller from controller.scs\n";
            return 0;
        }
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        auto target_state = [&ss,&nominal_trajectory_states,&target_left,&target_right](const state_type& x) {

            if(check_inside(target_left,target_right,x))
                return true;

            return false;
        };

        std::cout << "\nSimulation:\n " << std::endl;

        /* This part is used to see how many points of trajectory included in control domain
    for(int i=0;i<trajectory_size;i++)
         std::cout<<"check "<<i<<"- "<<con.check<state_type,input_type>(nominal_trajectory_states[i],GtoL,inside_of_area)<<std::endl;
    */


        state_type x=initial_trajectory_state;
        std::vector<state_type> syn_trajectory_states;
        syn_trajectory_states.push_back(x);

        std::cout<<"Closed loop from an specified initial point and disturbance"<<std::endl;

        while(1) {
            /*checking whether states is inside control domain or not*/
            if(!con.check<state_type,input_type>(x,GtoL,inside_of_area)){
                std::cout<<"This state is not inside of control domain"<<std::endl;
                break;
            }

            std::vector<input_type> u = con.get_controller<state_type,input_type>(x,GtoL,inside_of_area);
            std::cout << x[0] <<  " "  << x[1] <<"\n";
            ode_post_dist (x,u[0]);

            syn_trajectory_states.push_back(x);

            if(target_state(x) ) {
                std::cout << "Arrived: " << x[0] <<  " "  << x[1] << std::endl;
                break;
            }
        }

    }


    return 1;


}
