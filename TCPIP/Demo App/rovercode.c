#include "p32mx340f256h.h"

// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_ZEROCONF_LINK_LOCAL)
#include "TCPIP Stack/ZeroconfLinkLocal.h"
#endif
#if defined(STACK_USE_ZEROCONF_MDNS_SD)
#include "TCPIP Stack/ZeroconfMulticastDNS.h"
#endif

// Include functions specific to this stack application
#include "MainDemo.h"
#include "i2c.h"



void initi2Cs(void);

// Private helper functions.
// These may or may not be present in all applications.
void InitAppConfig(void);
void POSTTCPhead(int len,int id);
void InitializeBoard(void);
void POSTTCPchar(char b);
void ProcessIO(void);
void SendRiverData(void);
volatile int LINEA,LINEB;
int LINETa=512;	// if below this value then black line
int LINETb=512;

volatile unsigned int bitdelcount;
volatile char lastIR;
volatile int IRstreamHead,IRstreamTail,IRstreamLen;
volatile unsigned char IRres[512];
volatile unsigned char IRres2[512];


void __attribute((interrupt(ipl3), vector(_ADC_VECTOR), nomips16)) _ADCInterrupt(void)
{
  int i;
  if (lastIR==IR_IO)
    {if (bitdelcount<256)
     {
	 bitdelcount++;
	 if ((bitdelcount & 0xf)==0)
		{
			if (lastIR) IRres[IRstreamHead++]=0x80; else IRres[IRstreamHead++]=0x00;
			if (IRstreamHead>511) IRstreamHead=255;
		}
     }
    }
	else
	{lastIR=IR_IO;
	 if (bitdelcount>=256) 
        {
			if (IRstreamTail==0)
				{
					for (i=0;i<IRstreamHead;i++) IRres2[i]=IRres[i];
					IRstreamLen=IRstreamHead;
					IRstreamHead=IRstreamTail=0;
				}
		}
     bitdelcount=7;
    }	
  if (AD1CON2bits.BUFS)
		{
		   LINEA=ADC1BUF0;
		   LINEB=ADC1BUF1;
		}
		else
		{
		   LINEA=ADC1BUF8;
		   LINEB=ADC1BUF9;
		}
IFS1bits.AD1IF=0;
}


int riverOn=0;
int j=0;
int speed1=0x7fff;
int pos1=0, pos1ref=0;
int speed2=0x7fff;
int pos2=0, pos2ref=0;
int rCount=0;
int river[25]={0};
int speedSame=0;

	/*****************************************************************************/
	/*						HERE DANIEL RULES									 */								
	/*****************************************************************************/
//HERE WE STICK CONTROL... IT IS APPARENTLY SIMPLE.

void __attribute((interrupt(ipl2), vector(_TIMER_5_VECTOR), nomips16)) _T5Interrupt(void)
{
	IFS0bits.T5IF = 0;	
}

void __attribute((interrupt(ipl4), vector(_INPUT_CAPTURE_1_VECTOR), nomips16)) _IC1Interrupt(void)
{
if(speedSame&& pos1%5)
{
	if((pos1-pos1ref)!=(pos2-pos2ref))
	{
		speed1+=(pos2-pos2ref)-(pos1-pos1ref);
		
	}
}
if (speed1>0) 
{
	pos1++; 
	if(riverOn && pos1%2)
	{
		river[rCount]=(LINEA+LINEB)/2;
		
		if(rCount==24)
		{
			rCount=0;
			SendRiverData();
		}
		else
			rCount++;
	}
}
else pos1--;

IFS0bits.IC1IF=0;
}
void SendRiverData(void)
{
	POSTTCPhead(100,46);
	
	for(j=0; j<(25); j++)
	{
		POSTTCPchar(river[j]);
		POSTTCPchar(river[j]>>8);
	}
}
void __attribute((interrupt(ipl4), vector(_INPUT_CAPTURE_3_VECTOR), nomips16)) _IC3Interrupt(void)
{
if (speed2>0) pos2++; else pos2--;
if(speedSame&& pos2%5)
{
	if((pos2-pos2ref)!=(pos1-pos1ref))
	{
		speed2+=(pos1-pos1ref)-(pos2-pos2ref);
		
	}
}
IFS0bits.IC3IF=0;
}

#define comsdatasize 1024
char comsdata[comsdatasize];
unsigned int comsdatahead=0;
unsigned int comsdatatail=0;
void putcomsdata(char v)
{
  unsigned int nhead;
  nhead=comsdatahead+1;
  if (nhead>=comsdatasize) nhead-=comsdatasize;
  if (nhead!=comsdatatail)
	{
       comsdatahead=nhead;
       comsdata[comsdatahead]=v;
	}
}

char yescomsdata(void)
{
  if (comsdatatail!=comsdatahead) return(1); else return(0);
}

char getcomsdata(void)
{
  if (comsdatatail==comsdatahead) return(0);
  comsdatatail++;
  if (comsdatatail>=comsdatasize) comsdatatail-=comsdatasize;
  return(comsdata[comsdatatail]);
}

char peekcomsdata(void)
{
  int i;
  if (comsdatatail==comsdatahead) return(0);
  i=comsdatatail+1;
  if (i>=comsdatasize) i-=comsdatasize;
  return(comsdata[i]);
}

UINT16  globalRSSI=255;

void POSTTCPhead(int len,int id)
{
   POSTTCPchar(0xff);POSTTCPchar(len+2);POSTTCPchar(globalRSSI);POSTTCPchar(id);
}
int commandstate=-1;
int commandlen;
unsigned char nextcommand[256];
UINT16 REQCOUNT=0;
int waveid=0;

void initi2Cs(void)  // called once when rover starts
{
	//setup accn MMA8452Q
										I2S();I2send(0x38);I2send(0x2a);I2send(0x20);/*50Hz nonactive */ I2send(0x2);I2P();
										I2S();I2send(0x38);I2send(0x2a);I2send(0x21);/*50Hz active */ /*I2send(0x2);*/I2P();
	//setup ad MCP3425
										I2S();I2send(0xd0);I2send(0x90);I2P();
	//setup io MCP23008  // to be done
//										I2S();I2send(0x4e);I2send(0x90);I2P();
	//setup gyro L3G4200D  // to be done
//										I2S();I2send(0xd2);I2send(0x90);I2P();
//setup mag HMC5883L
										I2S();I2send(0x3c);I2send(0x0);I2send(0x70);I2send(0x20);I2send(0x00);I2P();
}


void setspeed(int newspeed1,int newspeed2)  // routine sets speed of motors 
											// only alters motors if speed has changed
{
 	if(!(newspeed1) && newspeed1==newspeed2)
  	{
		speedSame=1;
		pos1ref=pos1;
		pos2ref=pos2;
	}else
		speedSame=0;
  	if (newspeed1!=speed1)
	{
			speed1=newspeed1;
			  if (speed1==0) {LATD&=~0x006;OC3CON=0;OC2CON=0;OC3R=OC3RS=0;OC2R=OC2RS=0;}
			  if (speed1<0) 
				{	
					OC3CON=0;OC3R=OC3RS=0;
					OC2RS=abs(speed1);
					if (!OC2CON) OC2CON=0x8006;											 
				}
			  if (speed1>0) 
				{
					OC2CON=0;OC2R=OC2RS=0;
					OC3RS=abs(speed1);
					if (!OC3CON) OC3CON=0x8006;											 
				}
	}
  	if (newspeed2!=speed2)
	{
			speed2=newspeed2;
			  if (speed2==0) {LATD&=~0x0018;OC4CON=0;OC5CON=0;OC4R=OC4RS=0;OC5R=OC5RS=0;}
			
			  if (speed2<0) 
					{
						OC5CON=0;OC5R=OC5RS=0;
						OC4RS=abs(speed2);
						if (!OC4CON) OC4CON=0x8006;										 
					}
			  if (speed2>0) 
					{
						OC4CON=0;OC4R=OC4RS=0;
						OC5RS=abs(speed2);
						if (!OC5CON) OC5CON=0x8006;											 
					}
	}
}

int bptag=0;

void processcommand(void)		// the main routine which processes commands
{
 int i;
 unsigned char wk;
 int blocklen;
  switch (nextcommand[0]) // sort on command id (each case is for a different command)
  {
    case 0:POSTTCPhead(0,0);break;
	case 1:POSTTCPhead(1,1);    //Ask for Led and Switch status Packet
			POSTTCPchar(((~PORTE<<2) & 0xc)| ((~PORTD>>6) & 3)| ((LATE<<2) & 0x30));
			break;
	case 2:if (commandlen==1)		//Set LEDs
			{
			  if (nextcommand[1] & 2) LED1_IO=0; else LED1_IO=1;
			  if (nextcommand[1] & 1) LED0_IO=0; else LED0_IO=1;
			  POSTTCPhead(0,2);break;
			}
			break;
	case 3:POSTTCPhead(2,3);  // get internal counter
			POSTTCPchar(REQCOUNT & 0xff);
			POSTTCPchar((REQCOUNT>>8) & 0xff);REQCOUNT++;
			break;
	case 4:  // request motor status here 
			  POSTTCPhead(9,4);
			  POSTTCPchar(speed1);
			  POSTTCPchar(pos1);
			  POSTTCPchar(pos1>>8);

			  POSTTCPchar(speed2);
			  POSTTCPchar(pos2);
			  POSTTCPchar(pos2>>8);

			  POSTTCPchar(0);
			  POSTTCPchar(0);
			  POSTTCPchar(0);
			break;
	case 5:if (commandlen==1)	// set motor1
			{
			  setspeed((char)nextcommand[1],speed2);
			 POSTTCPhead(0,5);
			}
			break;
	case 6:if (commandlen==1)	// set motor2
			{
			  setspeed(speed1,(char)nextcommand[1]);
			 POSTTCPhead(0,6);
			}
			break;
	case 7:if (commandlen==2)	// set both motors
			{
			  setspeed((char)nextcommand[1],(char)nextcommand[2]);
			 POSTTCPhead(0,7);
			}
			break;
	case 8:	// report line following
			POSTTCPhead(5,8);
			i=0;
			if (LINEA<LINETa) i|=1;
			if (LINEB<LINETb) i|=2;
			POSTTCPchar(i);
			i=LINEA;
			POSTTCPchar(i);POSTTCPchar(i>>8);
			i=LINEB;
			POSTTCPchar(i);POSTTCPchar(i>>8);
			break;
	case 9:	if (commandlen==5)	// set line following thrsholds and LEDS
            {
			if (nextcommand[1]& 1) LED2_IO=1; else LED2_IO=0;
			if (nextcommand[1]& 2) LED3_IO=1; else LED3_IO=0;
			i=nextcommand[2]|(nextcommand[3]<<8);
			LINETa=i;
			i=nextcommand[4]|(nextcommand[5]<<8);
			LINETb=i;
			POSTTCPhead(0,9);
            }
			break;
	case 10:  
			 if (commandlen==1)	//request ultrasonic wave
			 {
			  I2S();I2send(0xB8);I2send(128);I2send(nextcommand[1]);
			  I2P();
			  POSTTCPhead(0,10);
			 } 
			break;
	case 11:  
			 if (commandlen==0)	//request utrasonic wave status
			 {
			  POSTTCPhead(1,11);
			  I2S();I2send(0xB8);I2send(128);
			  I2SR();I2send(0xb9);
			  POSTTCPchar(I2GET(0));
			  I2P();
			 } 
			waveid=0;
			break;
	case 12:  
			 if (commandlen==0)	//send ultrasonics wave section
			 {POSTTCPhead(86,12);
			  I2S();I2send(0xB8);I2send(129);I2SR();I2send(0xb9);
			  
			  POSTTCPchar(waveid++);
			  for (i=1;i<=85;i++)
				{POSTTCPchar(I2GET(i!=85));
				}
			 if (waveid==3) 
					bptag=1;
			  I2P();
			 } 
			break;
	case 13:  
			 if (commandlen==1)	//send ultrasonics trim
			 {
					  if ((nextcommand[1]>90) && (nextcommand[1]<110))
				{
			      I2S();I2send(0xB8);I2send(8);I2send(nextcommand[1]);
			      I2P();POSTTCPhead(0,13);
				}
			 } 
			break;
	case 16:  
			 if (commandlen==0)	//send ultrasonics
			 {POSTTCPhead(8,16);
			  I2S();I2send(0xB8);I2send(0);I2SR();I2send(0xb9);
			  for (i=1;i<=8;i++)
				{POSTTCPchar(I2GET(i!=8));
				}
			  I2P();
			 } 
			break;
	case 17:  
			 if (commandlen==0)	//send light/aux
			 {
			  I2S();I2send(0xB8);I2send(10);I2SR();I2send(0xb9);
			  POSTTCPhead(4,17);
			  for (i=1;i<=4;i++)
				{POSTTCPchar(I2GET(i!=4));
				}
			  I2P();
			 } 
			break;
	case 18:  
			 if (commandlen==3)	//pump init
			 {
			  POSTTCPhead(0,18);
			  I2S();I2send(0x12);I2send(0); I2send(nextcommand[1]);I2send(nextcommand[2]);I2send(nextcommand[3]);
			  I2P();
			 } 
			break;
	case 19:  
			 if (commandlen==1)	//pump write delta
			 {
			  POSTTCPhead(0,19);
			  I2S();I2send(0x12);I2send(0); I2send(nextcommand[1]);
			  I2P();
			 } 
			break;
	case 20:  
			 if (commandlen==0)	//pump read
			 {
			  POSTTCPhead(4,20);
			  I2S();I2send(0x12);I2send(0);I2SR();I2send(0x13);
			  for (i=1;i<=4;i++)
				{POSTTCPchar(I2GET(i!=4));
				}
			  I2P();
			 } 
			break;
	case 21:  
			 if (commandlen==1)	//POWER SWITCH
			 {
			  POSTTCPhead(0,21);
			  I2S();I2send(0x14);I2send(0); I2send(nextcommand[1]);
			  I2P();
			 } 
			break;
	case 22:  
			 if (commandlen==1)	//POWER SERV0 1
			 {
			  POSTTCPhead(0,22);
			  I2S();I2send(0x14);I2send(1); I2send(nextcommand[1]);
			  I2P();
			 } 
			break;
	case 23:  
			 if (commandlen==1)	//POWER SERV0 2
			 {
			  POSTTCPhead(0,23);
			  I2S();I2send(0x14);I2send(2); I2send(nextcommand[1]);
			  I2P();
			 } 
			break;
	case 24:  
			 if (commandlen==0)	//POWER READ AD0
			 {
			  POSTTCPhead(2,24);
			  I2S();I2send(0x14);I2send(8);I2SR();I2send(0x15);
			  for (i=1;i<=2;i++)
				{POSTTCPchar(I2GET(i!=2));
				}
			  I2P();
			 } 
			break;
	case 25:  
			 if (commandlen==0)	//POWER READ AD1
			 {
			  POSTTCPhead(2,25);
			  I2S();I2send(0x14);I2send(10);I2SR();I2send(0x15);
			  for (i=1;i<=2;i++)
				{POSTTCPchar(I2GET(i!=2));
				}
			  I2P();
			 } 
			break;
	case 26:  
			 if (commandlen==0)	//POWER READ AD6  - photo diode
			 {
			  POSTTCPhead(2,26);
			  I2S();I2send(0x14);I2send(12);I2SR();I2send(0x15);
			  for (i=1;i<=2;i++)
				{POSTTCPchar(I2GET(i!=2));
				}
			  I2P();
			 } 
			break;
	case 27:  
			 if (commandlen==0)	//POWER READ AD7  - thermistor
			 {
			  POSTTCPhead(2,27);
			  I2S();I2send(0x14);I2send(14);I2SR();I2send(0x15);
			  for (i=1;i<=2;i++)
				{POSTTCPchar(I2GET(i!=2));
				}
			  I2P();
			 } 
			break;
	case 28:  
			 if (commandlen==0)	//IR rx
			 {
 			  blocklen=(IRstreamLen+7)/8;
			  POSTTCPhead(blocklen,28);
			  for (i=1;i<=blocklen;i++)
				{
				 wk=IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
                 wk=(wk>>1) | IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
				 wk=(wk>>1) | IRres2[IRstreamTail++];
				 POSTTCPchar(wk);
				}
			  IRstreamLen =IRstreamTail=0;
			 } 
			break;
	case 40:  
			 if (commandlen==0)	//send accn MMA8452Q
			 {
				I2S();I2send(0x38);I2send(0x00);I2SR();I2send(0x39);
		    	i=I2GET(0);I2P();
			    if (i&8)
					{
					POSTTCPhead(6,40);
					I2S();I2send(0x38);I2send(0x01);I2SR();I2send(0x39);
					for (i=1;i<=6;i++)
						{POSTTCPchar(I2GET(i!=6));
						}
				    I2P();
					}
					else POSTTCPhead(0,40);
			 } 
			break;
	case 41:  
			 if (commandlen==0)	//send ad MCP3425
			 {
			    POSTTCPhead(3,41);
					I2S();I2send(0xd1);
					for (i=1;i<=3;i++)
						{POSTTCPchar(I2GET(i!=3));
						}
				    I2P();
			 } 
			break;
	case 42:  
			 if (commandlen==0)	//send io MCP23008
			 {
			    POSTTCPhead(11,42);
					I2S();I2send(0x4e);I2send(0x00);I2SR();I2send(0x4f);
					for (i=1;i<=11;i++)
						{POSTTCPchar(I2GET(i!=11));
						}
				    I2P();
			 } 
			break;
	case 43:  
			 if (commandlen==0)	//send gyro L3G4200D
			 {
			    POSTTCPhead(8,43);
					I2S();I2send(0xd2);I2send(0x26);I2SR();I2send(0xd3);
					for (i=1;i<=8;i++)
						{POSTTCPchar(I2GET(i!=8));
						}
				    I2P();
			 } 
			break;
	case 44:  
			 if (commandlen==0)	//send mag HMC5883L
			 {
			    POSTTCPhead(1,44);
					I2S();I2send(0x3c);I2send(9);I2SR();I2send(0x3d);
					POSTTCPchar(I2GET(0));
				    I2P();
			 } 
			break;
	case 45:  
			 if (commandlen==0)	//send mag HMC5883L
			 {
			    POSTTCPhead(6,45);
					I2S();I2send(0x3c);I2send(3);I2SR();I2send(0x3d);
					for (i=1;i<=6;i++)
						{POSTTCPchar(I2GET(i!=6));
						}
				    I2P();
			 } 
			break;
	case 46:
		if(commandlen==0) //send rover memory!
		{
			riverOn=1;
		}
		break;
		
  }
}

//Process packets
void ProcessIO(void)
{
 if (yescomsdata())
 {
  if (commandstate<0)
   {
     if ((unsigned char)getcomsdata()==0xff)
       {
         commandstate=0;
         if (yescomsdata())
			{
               commandstate=(unsigned char)getcomsdata();
			   commandlen=commandstate-1;
			   if (commandstate==0) commandstate=-1;
			}
	   }
   }
   else
   {
     if (commandstate==0)
		{
               commandstate=(unsigned char)getcomsdata();
			   commandlen=commandstate-1;
			   if (commandstate==0) commandstate=-1;
		}
		else
		{
			while (yescomsdata() && (commandstate))
				{
				  nextcommand[1+(commandlen-commandstate)]=getcomsdata();
				  commandstate--;
				}
			if (commandstate==0)
				{
					
					processcommand();
					commandstate=-1;
				}
		}
   }

 }



}


/****************************************************************************
  Function:
    static void InitializeBoard(void)

  Description:
    This routine initializes the hardware.  
  Precondition:
    None

  Parameters:
    None - None

  Returns:
    None

  Remarks:
    None
  ***************************************************************************/
void InitializeBoard(void)
{
    UINT32              actualClock;
	
  // init ports for analog  AN2 to AN3 are analog
AD1PCFG=0xfffffff3;
	IR_TRIS=1;IRstreamHead=IRstreamTail=0;
	lastIR=IR_IO;
	IC1_TRIS=1;
	IC2_TRIS=1;
	IC3_TRIS=1;
	IC4_TRIS=1;
TRISDbits.TRISD6=1;
TRISDbits.TRISD7=1;
	// LEDs
	LED0_TRIS = 0;
	LED1_TRIS = 0;

	LED2_TRIS = 0;	// used for line following
	LED3_TRIS = 0;
	
		
		// Enable multi-vectored interrupts
		INTEnableSystemMultiVectoredInt();
		
		// Enable optimal performance
		SYSTEMConfigPerformance(GetSystemClock());
		mOSCSetPBDIV(OSC_PB_DIV_1);				// Use 1:1 CPU Core:Peripheral clocks
		
		// Disable JTAG port so we get our I/O pins back, but first
		// wait 50ms so if you want to reprogram the part with 
		// JTAG, you'll still have a tiny window before JTAG goes away.
		// The PIC32 Starter Kit debuggers use JTAG and therefore must not 
		// disable JTAG.
		DelayMs(50);
		#if !defined(__MPLAB_DEBUGGER_PIC32MXSK) && !defined(__MPLAB_DEBUGGER_FS2)
			DDPCONbits.JTAGEN = 0;
		#endif
		T5CON=0;
		T5CONbits.TON=1;
		T5CONbits.TCKPS = 3;
				

		T3CON=0;
		PR3=8333;
        T3CONbits.TON=1;
		    AD1CHS = 2;					

	// ADC
	AD1CON1 = 0x0444;			// Turn on, auto sample start, convert on t3, 
	AD1CON2 = 0x0406;			// AVdd, AVss, int every 2 conversions, MUXA only, scan
	AD1CON3 = 0x1003;			// 16 Tad auto-sample, Tad = 3*Tcy
		AD1CSSL = 0xc;				// Scan line following
	AD1CON1 = 0x8444;			// Turn on, auto sample start, convert on t3, 


// Deassert all chip select lines so there isn't any problem with 
// initialization order.  Ex: When ENC28J60 is on SPI2 with Explorer 16, 
// MAX3232 ROUT2 pin will drive RF12/U2CTS ENC28J60 CS line asserted, 
// preventing proper 25LC256 EEPROM operation.
#if defined(WF_CS_TRIS)
	WF_CS_IO = 1;
	WF_CS_TRIS = 0;
#endif




	LeftMotorF=0;
	LeftMotorF_TRIS=0;
	LeftMotorB=0;
	LeftMotorB_TRIS=0;
	RightMotorF=0;
	RightMotorF_TRIS=0;
	RightMotorB=0;
	RightMotorB_TRIS=0;
	OC3CON=0;OC4CON=0;OC3R=OC3RS=0;OC4R=OC4RS=0;
	OC2CON=0;OC5CON=0;OC2R=OC2RS=0;OC5R=OC5RS=0;
	T2CON=0x8040;
	PR2=127;
	IC1CON=0x0001;
	IC1CON=0x8001;
	IC2CON=0x0001;
	IC2CON=0x8001;
	IC3CON=0x0001;
	IC3CON=0x8001;
	IC4CON=0x0001;
	IC4CON=0x8001;
IFS0bits.IC1IF=0;
IFS0bits.IC3IF=0;

IPC1bits.IC1IS=0;
IPC1bits.IC1IP=4;
IPC3bits.IC3IS=0;
IPC3bits.IC3IP=4;
IEC0bits.IC1IE=1;
IEC0bits.IC3IE=1;

IFS1bits.AD1IF=0;
IPC6bits.AD1IS=0;
IPC6bits.AD1IP=3;
IEC1bits.AD1IE=1;


    actualClock = I2CSetFrequency(I2C1, GetPeripheralClock(), 50000);
    // Enable the I2C bus
    I2CEnable(I2C1, TRUE);


}

