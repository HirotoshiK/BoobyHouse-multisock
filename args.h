

#ifndef __INC_ARGS_H
#define __INC_ARGS_H

#ifdef __cplusplus
extern "C" char getopts( const char *opt, int argc, char* argv[], char **argr );
extern "C" int is_numeric( char *str );
extern "C" int cut_and_conv_dec( const int n, const char *str, const char d, unsigned long *res );

#else
extern char getopts( const char *opt, int argc, char* argv[], char **argr );
extern int is_numeric( char *str );
extern int cut_and_conv_dec( const int n, const char *str, const char d, unsigned long *res );

#endif

#endif
