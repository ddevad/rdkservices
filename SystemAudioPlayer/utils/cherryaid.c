/* Cherry Audio Iterative Determinator - CherryAID */
#include <stdio.h>

#define DEBUG_OUTPUT

/* Stats */
unsigned long long acc_samples_in; /* 32bits give ~24hrs of samples before rollover. 64 bits and we never hve to worry*/
double elapsed_time_seconds; /* If this ever rolls over the viewer will be dead. */
unsigned long long acc_samples_out; /* As above */
double last_rx_time = 0.0;

#define SAMPLE_RATE_HZ 48000 /* Expected Sample Rate */

const double min_correction = 767.0/768.0;
const double max_correction = 769.0/768.0;
const double settle_time = 0.0; /* While the time rx_time is below this value the output is not corrected */

int cherryaid_init(void)
{
    elapsed_time_seconds = 0.0;
    acc_samples_out = 0;
    acc_samples_in = 0;
    last_rx_time = 0.0;

    return 0;
}

/*
    Called every for received buffer of samples.
    [in] samples_in : The number of samples contained within a buffer. It is assumed that every buffer is an integer number of samples
    [in] rx_time : The time (s) receorded when the samples where received. Time is expected to be relative to time zero. So the rx_time of the first
                    sample is also the time it took to receive. e.g. if 0.25s the receive time is 0.25 - 0. 
                    IMPORTANT. If the above is not true and time is an absolute receive time not relative to time zero, 
                    the algorithm will not work and will have to change.  
    [out] samples_out : The calculated number of samples that the input buffer of samples should be converted to compensate for drift

    Returns 0 if all is good. -1 if error.
*/
int cherryaid_new_data_point(unsigned int samples_in, double rx_time, unsigned int *samples_out)
{
    double expected_samples_in;
    double error_factor; /* acc_samples_out + samples_in - expected_rx_samples */
    double required_correction; /* 1 - error_factor/samples_in */
    double allowed_correction;
    double applied_correction;
    unsigned int corrected_samples_out;

    acc_samples_in += samples_in;

    expected_samples_in = rx_time * SAMPLE_RATE_HZ;

    error_factor = (double)(acc_samples_out + samples_in) - expected_samples_in;

    required_correction = (double)1.0 - error_factor/(double)samples_in;

    if( required_correction < min_correction )
        allowed_correction = min_correction;
    else if( required_correction > max_correction )
        allowed_correction = max_correction;
   
    corrected_samples_out = (unsigned int)(allowed_correction * (double)samples_in + 0.5);
   
    /* We only want to do this after the settling time has expired */
    if(rx_time > settle_time){
        *samples_out = corrected_samples_out; /* Report the number of sample that samples_in should be converted to */
    }
    else{
        *samples_out = samples_in; /* We are not changing the number of samples */
    }

    acc_samples_out += (unsigned long long) *samples_out; /* Update our global output count */

    last_rx_time = rx_time;

#ifdef DEBUG_OUTPUT
    printf("AccExpRx %lf ",expected_samples_in);
    printf("Rx_time %lf ",rx_time);
    printf("req_corr %lf ",required_correction);
    printf("ald_corr %lf ",allowed_correction);
    printf("samples_out %d ",*samples_out);
    printf("acc_samples_out %lld ",acc_samples_out);
    printf("AccExp-AccRx %lf ", expected_samples_in - (double)acc_samples_in);
    printf("AccExp-AccSampOut %lf ", expected_samples_in - (double)acc_samples_out);
    printf("\n");
#endif

    
}

int cherryaid_get_stats(double *total_time,
                        double *expected_samples_received,
                        unsigned long long *actual_samples_received,
                        unsigned long long *total_samples_output,
                        signed long long *uncorrected_error_in_samples,
                        signed long long *corrected_error_in_samples) 
{
    *total_time = last_rx_time;
    *expected_samples_received = last_rx_time * SAMPLE_RATE_HZ;
    *actual_samples_received = acc_samples_in;
    *total_samples_output = acc_samples_out;

    *uncorrected_error_in_samples = *expected_samples_received - acc_samples_in;
    *corrected_error_in_samples = *expected_samples_received - acc_samples_out;
/*
    *uncorrected_error_in_samples = *expected_samples_received;
    *corrected_error_in_samples = *expected_samples_received;*/
    return 0;
}
