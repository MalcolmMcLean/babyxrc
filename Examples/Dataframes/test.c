#include <stdio.h>
#include "temp.h"

int main(void)
{
  int i;

  for (i =0; i < 3; i++)
  {
    printf("%s %f %f\n", payroll[i].Name, payroll[i].Salary, 
payroll[i].PayrollID);  
  }

  return 0;
}
