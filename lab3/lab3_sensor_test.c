#include <stdio.h>
#include <stdlib.h>

void main()
{
   int iSystem;
   
   iSystem = system("./Lab3_i2c_BMP280.sh");
   iSystem = system("./Lab3_i2c_MPU9250.sh");
}
