#include <fstream>
#include <iostream>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
//#include <qt5/QtWidgets/QtWidgets>
/* SCOTS header */
#include "scots.hh"
//#include "PythonHandler.hh"
/* ode solver */
#include "RungeKutta4.hh"

/* time profiling */
#include "TicToc.hh"
/* memory profiling */
#include <ctime>
#include <sys/resource.h>

#include <map>
#include <unordered_map>

#define pi 3.14


#define obstacle_size 1
struct rusage usage;

/* state space dim */
const int state_dim=4;
/* input space dim */
const int input_dim=2;

//const int input_num=200;

/* sampling time */
const double tau =0.1;
//double epsilon=0.2;
int Tn=110;
int N2=110;

double M=0;

using state_type = std::array<double,state_dim>;
using input_type = std::array<double,input_dim>;
using abs_type = scots::abs_type;


state_type w={{-0.03,-0.03,-0.03}};
//state_type w={{0.03,0.03,0.03}};

state_type s_lb={{-0.5,0.1,-1,-0.1}};
state_type s_ub={{15.5,7.4,0.4,11}};
state_type s_eta={{0.02,0.02,0.02,0.1}};
state_type epsilon_num={7,7,7,0};

input_type i_lb={{-1,-2.2}};
/* upper bounds of the hyper rectangle */
input_type i_ub={{3,2.2}};
/* grid node distance diameter */
input_type i_eta={{0.3,0.15}};



//state_type epsilon={{0.5,0.5,0.5,0.3,0.3,}};


auto radius_post = [](state_type &r, const state_type &, const input_type &u) {
    //without disturbance
//    std::cout<<"M "<<M<<endl;
    //state_type w={{0.003,0.003}};
    r[0] = r[0] + ((r[2]+std::abs(w[2]))*std::abs(u[0]) + std::abs(w[0])) * tau;
    r[1] = r[1] + ((r[2]+std::abs(w[2]))*std::abs(u[0]) + std::abs(w[1])) * tau;
    r[2] = r[2] + std::abs(w[2]) * tau;
    r[3]=0;
};




bool check_inside(state_type left,state_type right, state_type point ){
    for (int i = 0; i < state_dim; ++i) {
        if(point[i] < left[i] || point[i] > right[i])
            return false;
    }
    return true;
}


void mapping3(std::vector<state_type>& trajectory_states,scots::UniformGrid ss, state_type s_eta
             ,state_type target_left,state_type target_right
             ,bool* inside_of_area
             ,std::vector<abs_type>& LtoG
             ,abs_type* GtoL
             ,std::vector<int>& trajectory_point){

    int dim=state_dim;
    abs_type index=0;
    std::vector<abs_type> lb(dim);  /* lower-left corner */
    std::vector<abs_type> ub(dim);  /* upper-right corner */
    std::vector<abs_type> no(dim);  /* number of cells per dim */
    std::vector<abs_type> cc(dim);  /* coordinate of current cell in the post */
    /* get radius */

    state_type r;
    state_type eta=s_eta;
    state_type m_z;
    /* for out of bounds check */
    state_type lower_left;
    state_type upper_right;
    /* fill in data */
    for(int i=0; i<dim; i++) {
     // r[i]=epsilon[i];
      r[i]=epsilon_num[i]*eta[i];

      lower_left[i]=ss.get_lower_left()[i];
      upper_right[i]=ss.get_upper_right()[i];
      m_z[i]=eta[i]/1e10;
    }
    /* determine post */

    for (int i = 0; i < Tn; ++i) {
    abs_type npost=1;
    state_type Left,Right;
    for(int k=0; k<dim; k++) {
      /* check for out of bounds */
      double left = trajectory_states[i][k]-r[k];
      double right = trajectory_states[i][k]+r[k];
      if(i==Tn){
          left = target_left[i];
          right = target_right[i];
      }
      Left[k]=left;
      Right[k]=right;
      if(left <= lower_left[k]-eta[k]/2.0  || right >= upper_right[k]+eta[k]/2.0) {
        std::cout<<"uniform grid is too small"<<std::endl;
      }
      /* integer coordinate of lower left corner of post */
      lb[k] = static_cast<abs_type>((left-lower_left[k]+eta[k]/2.0)/eta[k]);
      /* integer coordinate of upper right corner of post */
      ub[k] = static_cast<abs_type>((right-lower_left[k]+eta[k]/2.0)/eta[k]);
      /* number of grid points in the post in each dimension */
      no[k]=(ub[k]-lb[k]+1);
      /* total number of post */
      npost*=no[k];
      cc[k]=0;
    }

    /* compute indices of post */
    for(abs_type k=0; k<npost; k++) {
      abs_type q=0;
      for(int l=0; l<dim; l++)  {
        q+=(lb[l]+cc[l])*ss.get_nn()[l];
      }
      cc[0]++;
      for(int l=0; l<dim-1; l++) {
        if(cc[l]==no[l]) {
          cc[l]=0;
          cc[l+1]++;
        }
      }
      if(inside_of_area[q]==0){
          LtoG.push_back(q);
          trajectory_point.push_back(i);
          state_type x;
          ss.itox(q,x);
          GtoL[q]=index;
          inside_of_area[q]=1;
          index++;
      }
    }
    }
    abs_type npost=1;
    state_type Left,Right;
    for(int k=0; k<dim; k++) {
      /* check for out of bounds */
      double left = target_left[k];
      double right = target_right[k];

      Left[k]=left;
      Right[k]=right;
      if(left <= lower_left[k]-eta[k]/2.0  || right >= upper_right[k]+eta[k]/2.0) {
        std::cout<<"state uniform grid is too small"<<std::endl;
        std::cout<<"Left: "<<left<<"Right: "<<right<<std::endl;
      }
      /* integer coordinate of lower left corner of post */
      lb[k] = static_cast<abs_type>((left-lower_left[k]+eta[k]/2.0)/eta[k]);
      /* integer coordinate of upper right corner of post */
      ub[k] = static_cast<abs_type>((right-lower_left[k]+eta[k]/2.0)/eta[k]);
      /* number of grid points in the post in each dimension */
      no[k]=(ub[k]-lb[k]+1);
      /* total number of post */
      npost*=no[k];
      cc[k]=0;
    }

    /* compute indices of post */
    for(abs_type k=0; k<npost; k++) {
      abs_type q=0;
      for(int l=0; l<dim; l++)  {
        q+=(lb[l]+cc[l])*ss.get_nn()[l];
      }
      cc[0]++;
      for(int l=0; l<dim-1; l++) {
        if(cc[l]==no[l]) {
          cc[l]=0;
          cc[l+1]++;
        }
      }
      if(inside_of_area[q]==0){
          state_type x;
          ss.itox(q,x);
          LtoG.push_back(q);
          trajectory_point.push_back(Tn);
          GtoL[q]=index;
          inside_of_area[q]=1;
          index++;
      }
    }

    std::cout<<"Reduced size: "<<index<<std::endl;
}







auto  uni_post = [](state_type &x, const input_type &u) {
    /* the ode describing the vehicle */

    auto rhs =[](state_type& xx,  const state_type &x, const input_type &u) { // state space model

        xx[0] = u[0]*std::cos(x[2]);
        xx[1] = u[0]*std::sin(x[2]);

        xx[2] = u[1];
        xx[3] = 1;


    };
    /* simulate (use 10 intermediate steps in the ode solver) */
    scots::runge_kutta_fixed4(rhs,x,u,state_dim,tau,10);
};
auto  uni_post_dist = [](state_type &x, const input_type &u) {
    /* the ode describing the vehicle */

    auto rhs =[](state_type& xx,  const state_type &x, const input_type &u) { // state space model

        xx[0] = u[0]*std::cos(x[2])+w[0];
        xx[1] = u[0]*std::sin(x[2])+w[1];
        xx[2] = u[1]+w[2];
        xx[3] = 1;


    };
    /* simulate (use 10 intermediate steps in the ode solver) */
    scots::runge_kutta_fixed4(rhs,x,u,state_dim,tau,10);
};















int main(){
    srand (time(NULL));

    /* to measure time */
    TicToc tt;
    TicToc tt2;

    /* setup the workspace of the synthesis problem and the uniform grid */
    /* lower bounds of the hyper rectangle */


    /* reading nominal controller from a file and simulating   */
    //__________________________________________________________________________________________________

    std::ifstream inFile;
    inFile.open("/home/mzareian/Scots+Altro2/examples/merge/tr_U.txt");
    int trajectory_number=Tn;
    std::vector<input_type> u_temp(6);

    int i=0;

    input_type min_u={{1000,1000}};
    input_type max_u={{-1000,-1000}};




    std::vector<input_type> inputs(Tn);

    for ( int i = 0; i <Tn ; i++) {
        for (int k = 0; k < 6; ++k) {
            inFile >> u_temp[k][0] >>u_temp[k][1];
        }
      inputs[i]=u_temp[3];
      for (int j = 0; j < state_dim; ++j) {
          if(min_u[j]>inputs[i][j])
              min_u[j]=inputs[i][j];
          if(max_u[j]<inputs[i][j])
              max_u[j]=inputs[i][j];

      }
    }
    state_type initial_trajectory_state;

//     initial_trajectory_state={{6.0,1.2,0,0}};//1
//    initial_trajectory_state={{3.0,1.2,0,0}};//2
//    initial_trajectory_state={{0.0,1.2,0,0}};//3
    initial_trajectory_state={{8,3.9,-0.64,0}};//4
//    initial_trajectory_state={{6,5.4,-0.64,0}};//5
//    initial_trajectory_state={{4.0,6.9,-0.64,0}};//6


    std::vector<state_type> trajectory_states;
    std::vector<state_type> trajectory_states_dist;

    trajectory_states.push_back(initial_trajectory_state);
    trajectory_states_dist.push_back(initial_trajectory_state);
    state_type temp_state=initial_trajectory_state;
    state_type temp_state2=initial_trajectory_state;


    state_type min_x=initial_trajectory_state;
    state_type max_x=initial_trajectory_state;


    for ( int i = 0; i <trajectory_number-1 ; i++)
    {

        uni_post (temp_state,inputs[i]);
        uni_post_dist (temp_state2,inputs[i]);
        if(temp_state[0]>15){
            trajectory_number=i+1;
            Tn=i+1;
            break;
        }

        for (int j = 0; j < state_dim; ++j) {
            if(min_x[j]>temp_state[j])
                min_x[j]=temp_state[j];
            if(max_x[j]<temp_state[j])
                max_x[j]=temp_state[j];
        }
        trajectory_states.push_back(temp_state);
        trajectory_states_dist.push_back(temp_state2);
        std::cout<<i+1<<"- " << trajectory_states[i+1][0]<< "  " <<trajectory_states[i+1][1]<<"  " <<trajectory_states[i+1][2]<<"  "<<trajectory_states[i+1][3] << std::endl;
    } 
    for(int i=0; i<Tn;i++)
       for(int k=0;k<3;k++)
          if(std::abs(trajectory_states_dist[i][k]-trajectory_states[i][k])>(epsilon_num[k]+1)*s_eta[k])
             std::cout<<i<<" mismatch "<<k<<" "<<trajectory_states_dist[i][k]<<std::endl;

    
    
    for (int i = 0; i < state_dim; ++i)
        std::cout<<i<<" min:"<<min_x[i]<<" max:"<<max_x[i]<<std::endl;
    for (int i = 0; i < state_dim; ++i) {
        min_x[i]=min_x[i]-6*s_eta[i];
        max_x[i]=max_x[i]+6*s_eta[i];
    }
    //__________________________________________________________________________________________________


    state_type target_left;
    state_type target_right;
   
    for (int i = 0; i < state_dim; ++i) {
     double target_radius=epsilon_num[i]*s_eta[i];
     if(i==3)
        target_radius=s_eta[i]/100.0;
     target_left[i]=trajectory_states[Tn-1][i]-target_radius;
     target_right[i]=trajectory_states[Tn-1][i]+target_radius;
    }



    scots::UniformGrid ss(state_dim,s_lb,s_ub,s_eta);
    std::cout << "Uniform grid details:" << std::endl;
    ss.print_info();


    for (int i = 0; i < input_dim; ++i) {
        min_u[i]=min_u[i]-i_eta[i];
        max_u[i]=max_u[i]+i_eta[i];
    }
   // input_type i_lb=min_u;
   // input_type i_ub=max_u;

    scots::UniformGrid is(input_dim,i_lb,i_ub,i_eta);
    is.print_info();



    long long unsigned int N=ss.size();
    bool* inside_of_area= new bool[N];
//    std::vector<bool> inside_of_area(N);
    abs_type* GtoL=new abs_type[N];
//    std::vector<abs_type>  GtoL(N);
    std::vector<abs_type>  LtoG;
    std::vector<int>  trajectory_point_number;

   // distance_finder(trajectory_states,ss,s_eta,inputs);
    tt2.tic();
    mapping3(trajectory_states,ss,s_eta,target_left,target_right,inside_of_area,LtoG,GtoL,trajectory_point_number);
    tt2.toc();


    std::cout << "Computing the transition function: " << std::endl;
//     /* transition function of symbolic model */
    scots::TransitionFunction tf;
    scots::Abstraction<state_type,input_type> abs(ss,is);



    auto avoid = [trajectory_states,ss,s_eta](const abs_type& idx) {

        return false;
    };


//    /* 3rd version Abstraction */
    tt.tic();
     abs.compute_gb2_temp(tf,uni_post, radius_post, avoid,inside_of_area,LtoG,GtoL);
    tt.toc();
    if(!getrusage(RUSAGE_SELF, &usage))
        std::cout << "Memory per transition: " << usage.ru_maxrss/(double)tf.get_no_transitions() << std::endl;
    std::cout << "Number of transitions: " << tf.get_no_transitions() << std::endl;

    std::cout << "mem :" << usage.ru_maxrss<< std::endl;

    /* define target set */
    auto target_index = [&ss,&s_eta,&trajectory_states,&LtoG,&target_left,&target_right](const abs_type& idx) {
        state_type x;
        ss.itox(LtoG[idx],x);
        if(check_inside(target_left,target_right,x))
            return true;
        return false;
    };



    std::cout << "\nSynthesis: " << std::endl;
    tt.tic();
    scots::WinningDomain win=scots::solve_reachability_game(tf,target_index);
    tt.toc();
    std::cout << "Winning domain size: " << win.get_size() << std::endl;

    std::cout << "\nWrite controller to controller.scs \n";
    if(write_to_file(scots::StaticController(ss,is,std::move(win)),"controller"))
        std::cout << "Done. \n";

    scots::StaticController con;
      if(!read_from_file(con,"controller")) {
        std::cout << "Could not read controller from controller.scs\n";
        return 0;
      }
      //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      auto target2 = [&ss,&s_eta,&trajectory_states,&target_left,&target_right](const state_type& x) {

          if(check_inside(target_left,target_right,x))
              return true;

          return false;
        };

      std::cout << "\nSimulation:\n " << std::endl;
       for(int i=0;i<Tn;i++)
          std::cout<<"check "<<i<<"- "<<con.check<state_type,input_type>(trajectory_states[i],GtoL,inside_of_area)<<std::endl;

    state_type x=initial_trajectory_state;

    std::vector<state_type> syn_trajectory_states;
    syn_trajectory_states.push_back(x);
    int k=0;
    while(1) {
      if(!con.check<state_type,input_type>(x,GtoL,inside_of_area))
          break;
    //std::vector<input_type> u = con.get_control<state_type,input_type,input_set>(x,GtoL,inside_of_area,InputSet);
      std::vector<input_type> u = con.get_control<state_type,input_type>(x,GtoL,inside_of_area);
      std::cout<<k<<" - " << x[0] <<  " "  << x[1] <<" "<<x[3]<<"\n";
     
      for(int i=0;i<state_dim;i++)
        if(std::abs(x[i]-trajectory_states[k][i])>(epsilon_num[i]+1)*s_eta[i])
         std::cout<<"mismatch"<<i<<std::endl;

      uni_post_dist (x,u[0]);

      syn_trajectory_states.push_back(x);

      if(target2(x) ) {//||target2(x)) {
        std::cout << "Arrived: " << x[0] <<  " "  << x[1] << std::endl;
        break;
      }
      k++;
    }


    std::ofstream outfile ("T_c.txt",std::ofstream::binary);
    for(int i=0; i<syn_trajectory_states.size();i++){
        outfile<<syn_trajectory_states[i][0]<<std::endl;
        outfile<<syn_trajectory_states[i][1]<<std::endl;
        outfile<<syn_trajectory_states[i][2]<<std::endl;
    }

    return 1;


}
