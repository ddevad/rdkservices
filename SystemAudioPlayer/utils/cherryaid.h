
int cherryaid_new_data_point(unsigned int samples_in, double time, unsigned int *samples_out);
int cherryaid_init(void);
int cherryaid_get_stats(double *total_time,
                        double *expected_samples_received,
                        unsigned long long *actual_samples_received,
                        unsigned long long *total_samples_output,
                        signed long long *uncorrected_error_in_samples,
                        signed long long *corrected_error_in_samples);
void cherryaid_write_wav(const char* filename, const char* data, int len);
