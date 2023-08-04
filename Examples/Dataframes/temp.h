#ifndef script_h
#define script_h

/*
Dataframes.
Payroll: a dataframe with mixed data and a header
Numbers: a dataframe which is a raw matrix of numbers.
Strings: a dataframe which is just string.
*/
typedef struct
{
	const char *Name;
	double Salary;
	double PayrollID;
}PAYROLL;

extern PAYROLL payroll[3];
extern double numbers[3][4];
extern const char *strings[3][4];

#endif
