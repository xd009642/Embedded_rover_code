#include "proc\p32mx340f256h.h"
#include "TCPIP Stack/TCPIP.h"


char     i2cerror=0;


void I2S(void)
{
  I2C1CON=I2C1CON & 0xffe0;
 I2C1CONbits.SEN=1;
 while (I2C1CONbits.SEN);
}

void I2SR(void)
{
  I2C1CON=I2C1CON & 0xffe0;
 I2C1CONbits.RSEN=1;
 while (I2C1CONbits.RSEN);
}

void I2send(int v)
{
  int i;
  IFS0bits.I2C1MIF=0;
  I2C1TRN=v;
  i=10000;
  while ((i) && (I2C1STATbits.TRSTAT)) i--; /* wait till transmitted*/
  i=10000;
  while ((i) && (!IFS0bits.I2C1MIF)) i--; /* wait till transmitted*/
  IFS0bits.I2C1MIF=0;	
}

int I2GET(int ack)
{
  int i;
  int readval;
  I2C1CON=I2C1CON & 0xffe0;
//  readval=I2CRCV;
  I2C1CONbits.RCEN=1;
  i=10000;
  while ((i) && (I2C1CONbits.RCEN)) i--; 
  if (!i) i2cerror=1;
//  readval=I2CRCV;
  if (!ack) 
	{I2C1CONbits.ACKDT=1;}
	 else 
	{I2C1CONbits.ACKDT=0;}
  I2C1CONbits.ACKEN=1;
  while (I2C1CONbits.ACKEN);
  readval=I2C1RCV;
  I2C1CON=I2C1CON & 0xffe0;
 return(readval);
}

void I2P(void)
{
  I2C1CON=I2C1CON & 0xffe0;
 I2C1CONbits.PEN=1;
 while (I2C1CONbits.PEN);
 if (i2cerror)
  {
    I2CEnable(I2C1, FALSE);
    i2cerror=0;
    I2CEnable(I2C1, TRUE);
  }
}


