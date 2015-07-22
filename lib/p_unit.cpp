#include <math.h>
#include "config.h"
#include "p_unit.h"
#include <iostream>

/*******************************************************************************
POTTS UNIT
*******************************************************************************/
PUnit::PUnit( const int & S, const int & C, const int & N){
    this->state = new __fpv[S + 1];
    this->cdata = new __fpv[S * N * S];
    this->r = new __fpv[S + 1];
    this->S = S;
    this->C = C;
    this->N = N;
    this->h = new __fpv[S];
    this->theta = new __fpv[S];
    this->lc_cdata = new __fpv[S * C * S];

}

PUnit::~PUnit(){
    delete[] this->state;
    delete[] this->cdata;
    delete[] this->r;
    delete[] this->h;
    delete[] this->theta;
    delete[] this->lc_cdata;
}

__fpv * PUnit::get_state(){
    return this->state;
}
void PUnit::init_states(const __fpv & beta, const __fpv & U){

    int i;
    __fpv n = -2 * beta - 2 * exp(beta * U) - 2 * S+sqrt(pow(2 * beta + 2 * exp(beta * U)+2 * S,2)+8 * (-beta * beta - 2 * beta * S + 2 * beta *S * exp(beta * U)));
    __fpv d = 2 * (-beta * beta - 2 * beta * S + 2 * beta * S * exp(beta * U));

    for(i = 0; i < S; ++i){
        state[i] = n / d;
    }

    state[S] = 1 - S*state[0];
    r[S] = 1 - state[S];

}

void PUnit::init_J(const int & p, const __fpv & a, const int * xi, const int & unit, const int * ucm, PUnit ** network){

    int i,j,k,l;
    __fpv as = a/S;

    //Generate Jkxl
    for(i = 0; i < S; ++i){
        for(j = 0; j < N; ++j){
            for(k = 0; k < S; ++k){

                cdata[N*S*i + S*j + k] = 0;

                if(ucm[unit*N + j] == 1){

                    for(l = 0; l < p; ++l){
                        cdata[N*S*i + S*j + k] += ((xi[p * unit + l]==i)-as)*((xi[p * j + l]==k)-as);
                    }

                    cdata[N*S*i + S*j + k] /= a * (1 - as)* C;

                    h[i] += cdata[N*S*i + S*j + k] * network[j]->state[k];
                }

            }
        }

        r[i] = h[i];
        theta[i] = state[i];

    }


}

void PUnit::update_rule(const int & init_pattern, const __fpv * states, const __fpv & U, const __fpv & w, const __fpv & g, const __fpv & tau, const __fpv & b1, const __fpv & b2, const __fpv & b3, const __fpv & beta, const int & tx, const int & t){


    //tx == n0 in the old code, "time 'x' "
    int i,j;
    __fpv self=0, INcost, rmax, Z;
    int tsize = this->N * this->S;

    rmax = this->r[this->S];

    for(i = 0; i < this->S; ++i){
        self += this->state[i];
        h[i] = 0;
    }
    self = (w / this->S) * self;

    INcost = (t > tx) * g * exp(-(t-tx)/tau);

    for(i = 0; i < this->S; ++i){

        for(j = 0; j < tsize; ++j){
            this->h[i] += this->cdata[N*S*i + j] * states[j];
        }

        this->h[i] += w * this->state[i] - self + INcost * (init_pattern == i);

        this->theta[i] += b2 * (this->state[i]-this->theta[i]);
	    this->r[i] += b1 * (this->h[i]-this->theta[i]-this->r[i]);

        rmax = r[i] * (r[i] > rmax) - ((r[i] > rmax) - 1) * this->r[this->S];

    }

    this->r[S] += b3 * (1 - this->state[S] - this->r[S]);

    Z=0;

    for(i = 0; i < S; ++i){
        Z += exp(beta * (r[i] - rmax));
    }

    Z += exp(beta * (r[S] + U - rmax));

    for(i = 0; i < S; ++i){
    	this->state[i] = exp(beta * (this->r[i] - rmax)) / Z;
    }

    this->state[S]=exp(beta * (this->r[S] - rmax + U)) / Z;

}


void PUnit::lc_init_J(const int & p, const __fpv & a, const int * xi, const int & unit, const int * cm, PUnit ** network){

    int i,j,k,l;
    double as = a/S;

    //Generate Jkxl
    for(i = 0; i < S; ++i){
        h[i] = 0;
        for(j = 0; j < C; ++j){
            for(k = 0; k < S; ++k){

                lc_cdata[C*S*i + S*j + k] = 0;
                //cdata[i * (2*C*S) + j * (S) + k] = 0;

                //Half filled with Jkxl
                for(l = 0; l < p; ++l){
                    lc_cdata[C*S*i + S*j + k] += ((xi[p * unit + l]==i)-as)*((xi[p * cm[C*unit+j] + l]==k)-as);
                }
                lc_cdata[C*S*i + S*j + k] /= a * (1 - as)* C;

                //Compute directly initial h
                h[i] += lc_cdata[C*S*i + S*j + k] * network[cm[C*unit + j]]->state[k];
            }
        }
        r[i] = h[i];
        theta[i] = state[i];
    }

}


void PUnit::lc_update_rule(const int & init_pattern, const __fpv buffer[], const __fpv & U, const __fpv & w, const __fpv & g, const __fpv & tau, const __fpv & b1, const __fpv & b2, const __fpv & b3, const __fpv & beta, const int & tx, const int & t){


    //tx == n0 in the old code, "time 'x' "
    int i,j;
    __fpv self=0, INcost, rmax, Z;
    int tsize = this->C * this->S;

    rmax = this->r[this->S];

    for(i = 0; i < this->S; ++i){
        self += this->state[i];
        h[i] = 0;
    }
    self = (w / this->S) * self;

    INcost = (t > tx) * g * exp(-(t-tx)/tau);

    for(i = 0; i < this->S; ++i){

        for(j = 0; j < tsize; ++j){
            this->h[i] += this->lc_cdata[C*S*i + j] * buffer[j];
        }

        this->h[i] += w * this->state[i] - self + INcost * (init_pattern == i);

        this->theta[i] += b2 * (this->state[i]-this->theta[i]);
	    this->r[i] += b1 * (this->h[i]-this->theta[i]-this->r[i]);

        rmax = r[i] * (r[i] > rmax) - ((r[i] > rmax) - 1) * this->r[this->S];

    }

    this->r[S] += b3 * (1 - this->state[S] - this->r[S]);

    Z=0;

    for(i = 0; i < S; ++i){
        Z += exp(beta * (r[i] - rmax));
    }

    Z += exp(beta * (r[S] + U - rmax));

    for(i = 0; i < S; ++i){
    	this->state[i] = exp(beta * (this->r[i] - rmax)) / Z;
    }

    this->state[S]=exp(beta * (this->r[S] - rmax + U)) / Z;

}
