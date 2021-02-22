
#include <vector>
#define verbose true
#define pi 3.14


/* state space dim */
const int state_dim=5;
/* input space dim */
const int input_dim=1;
/*
 * data types for the state space elements and input space
 * elements used in uniform grid and ode solvers
 */
using state_type = std::array<double,state_dim>;
using input_type = std::array<double,input_dim>;

using abs_type = scots::abs_type;


/*Assigning values to parametrs*/
class Parameters {
  public:

    const char* address="/home/mehrdad/Final_Project/examples/formation/tr_U.txt";

    /* lower bounds of the hyper rectangle */
state_type s_lb={{-2,-2,-0.6,-0.1}};
    /* upper bounds of the hyper rectangle */
state_type s_ub={{17,17,1.6,10}};
    /* grid node distance diameter */
state_type s_eta={{0.04,0.04,0.02,0.08,0.1}};

    /* lower bounds of the hyper rectangle */
input_type i_lb={-7};
    /* upper bounds of the hyper rectangle */
input_type i_ub={7};
    /* grid node distance diameter */
input_type i_eta={{0.2}};

    /*Disturbance vector(for each state)*/
state_type w={{0,0.05,0,0,0}};

    /*sampling time*/
    const double tau =0.1;

    /*number of samples in nominal trajectory (trajectory is discrete time)*/
    int trajectory_size=70;
    /* tube size is equal to number of boxes times */

    state_type epsilon_num={8,10,10,11,0};

    const int state_dim=5;
    const int input_dim=1;

    const int trajectory_num=1;
    const int agent_num=1;
    std::vector<state_type> initial_trajectory_states{ {{0,0,pi,0,0}}   //1

                                                     };

} parameters;





/* we integrate the growth bound by tau sec (the result is stored in r)  */
inline auto radius_post = [](state_type &r, const state_type &, const input_type &u) {
    //without disturbance
    static state_type w=parameters.w;
    static double tau=parameters.tau;
    r[0] = r[0] + 0.15*r[1]+ 0.027*r[2]+0.0063*r[3]+0.15*w[0]+0.011*w[1]+0.0012*w[2];
    r[1] = r[1] + 0.41*r[2]+0.098*r[3]+0.15*w[1]+0.025*w[2]+0.005*w[3];
    r[2] = 1.20*r[2] + 0.15*r[3]+w[2]*0.16+0.011*w[3] ;
    r[3]= 2.75 *r[2]+ 1.15*r[3]+0.21*w[2]+0.15*w[3];
    r[4]=0;
};
//std::abs(






/* we integrate the unicycle ode by tau sec (the result is stored in x)  */
inline auto  ode_post = [](state_type &x, const input_type &u) {
    /* the ode describing the vehicle */
    double tau=parameters.tau;

    auto rhs =[](state_type& xx,  const state_type &x, const input_type &u) { // state space model

        double g = 9.8;
        double M_c=1.0;
        double M_p=0.1;
        double M_t=M_p+M_c;
        double l=0.5;
        double c1 = u[0]/M_t;
        double c2 = M_p*l/M_t;
        double c3=l*4.0/3.0;
        double c4=l*M_p/M_t;

        double F=(g*std::sin(x[2])-std::cos(x[2])*(c1+c2*x[3]*x[3]*std::sin(x[2])))/(c3-c4*std::cos(x[2])*std::cos(x[2]));
        double G= (c1+c2*x[3]*x[3]*std::sin(x[2])) - c4* std::cos(x[2])*F;

        xx[0] =x[1];
        xx[1] =G;
        xx[2] =x[3];
        xx[3] =F;
        xx[4]=1;

    };
    /* simulate (use 10 intermediate steps in the ode solver) */
    scots::runge_kutta_fixed4(rhs,x,u,state_dim,tau,1);

};


auto  ode_post_dist = [](state_type &x, const input_type &u) {
    /* the ode describing the vehicle */
    double tau=parameters.tau;
    auto rhs =[](state_type& xx,  const state_type &x, const input_type &u) { // state space model
        double g = 9.8;
        double M_c=1.0;
        double M_p=0.1;
        double M_t=M_p+M_c;
        double l=0.5;
        double c1 = u[0]/M_t;
        double c2 = M_p*l/M_t;
        double c3=l*4.0/3.0;
        double c4=l*M_p/M_t;

        double F=(g*std::sin(x[2])-std::cos(x[2])*(c1+c2*x[3]*x[3]*std::sin(x[2])))/(c3-c4*std::cos(x[2])*std::cos(x[2]));
        double G= (c1+c2*x[3]*x[3]*std::sin(x[2])) - c4* std::cos(x[2])*F;

        xx[0] =x[1]+w[0];
        xx[1] =G+w[1];
        xx[2] =x[3]+w[2];
        xx[3] =F+w[3];
        xx[4]=1;

    };
    /* simulate (use 10 intermediate steps in the ode solver) */
    scots::runge_kutta_fixed4(rhs,x,u,state_dim,tau,10);
};

std::vector<state_type> trajectory_simulation(Parameters& parameters){
    std::vector<state_type> nominal_trajectory_states;

    int trajectory_size=parameters.trajectory_size;
    int tr_num=parameters.trajectory_num;
    state_type initial_trajectory_state=parameters.initial_trajectory_states[tr_num];
    std::ifstream inFile;
    inFile.open(parameters.address);


    std::vector<input_type> u_temp(parameters.agent_num);
    std::vector<input_type> inputs(trajectory_size);

    for ( int i = 0; i <trajectory_size ; i++) {
        for (int k = 0; k < parameters.agent_num ; ++k) {
            for(int q=0;q<input_dim;q++)
                inFile >> u_temp[k][q];
        }
      inputs[i]=u_temp[tr_num];
    }

    state_type temp_state;
    nominal_trajectory_states.push_back(initial_trajectory_state);
    temp_state=initial_trajectory_state;
    state_type min_x=initial_trajectory_state;
    state_type max_x=initial_trajectory_state;


    for ( int i = 0; i <trajectory_size-1 ; i++)
    {
        ode_post (temp_state,inputs[i]);
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








