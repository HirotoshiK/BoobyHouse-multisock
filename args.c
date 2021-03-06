
#include <stdio.h>
#include "args.h"

char getopts( const char *opt, int argc, char* argv[], char **argr )
{
	static int point = 0;
	char *p;
	int num = 0;
	char oc = '\0';

	point++;
	p = argv[point];

	if ( point >= argc ) {
		point = 0;
	}
	else if ( *p == '-' ) {
		oc = *( p + 1 );
		while ( *opt ) {
			if ( *opt == oc ) {
				if ( *( opt + 1 ) == ':' ) {
					if ( *( p + 2 ) == '\0' ) {
						point++;
						if ( point > argc ) {
							*argr = "";
						}
						else {
							*argr = argv[point];
						}
					}
					else {
						*argr = p + 2;
					}
				}
				else {
					*argr = NULL;
				}
				break;
			}
			if ( *opt != ':' ) {
				num++;
			}
			opt++;
		}
	}

	return oc;
}

int is_numeric( char *str )
{
	char *p = str;
	int flag = 10;	// dec

	if ( *p == '0' ) {
		p++;
		if ( *p != '\0' ) {
			flag = 8;	// oct
			if ( *p == 'x' || *p == 'X' ) {
				flag = 16;	// hex
				p++;
			}
		}
	}
	for ( ; *p ; p++ ) {
		switch ( flag ) {
		case 8:
			if ( *p < '0' || *p > '7' ) {
				return -1;
			}
			break;
		case 10:
			if ( *p < '0' || *p > '9' ) {
				return -1;
			}
			break;
		case 16:
			if ( *p >= '0' && *p <= '9' ) {
				break;
			}
			if ( *p >= 'a' && *p <= 'f' ) {
				break;
			}
			if ( *p >= 'A' && *p <= 'F' ) {
				break;
			}
			return -1;
		}
	}
	return flag;
}

int cut_and_conv_dec( const int n, const char *str, const char d, unsigned long *res )
{
	int c = 1;
	char *p = (char*)str;
	unsigned long val = 0;

	while ( *p ) {
		if ( *p == d ) {
			if ( c == n ) {
				*res = val;
				return 0;
			}
			else {
				val = 0;
			}
			c++;
		}
		else if ( *p >= '0' && *p <= '9' ){
			val = val * 10 + (unsigned long)(*p - '0');
		}
		else {
			return -1;
		}
		p++;
	}
	if ( c == n ) {
		*res = val;
		return 0;
	}
	else {
		*res = 0;
		return 0;
	}
}
