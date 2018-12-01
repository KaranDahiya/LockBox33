#include "gpiolib_addr.h"
#include "gpiolib_reg.h"
#include "gpiolib_reg.c"

#include <stdint.h>
#include <linux/watchdog.h> 
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

//4 Diode 1, 9 Diode 2, 16 Red LED, 19 Green LED Lasers, 17 Alarm
//18 for the servo motor
//21 for the capacitance

GPIO_Handle initializeGPIO()
{
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if(gpio == NULL)
	{
		perror("Could not initialize GPIO");
	}
	return gpio;
}

int pinCheck(GPIO_Handle gpio, int pinNumber){
	//in this code, all the pins are in register 0, so we dont need to worry what the value in the GPLEV is 
	if(pinNumber < 0 || pinNumber > 9){
		return -1;
	}
	uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));
	printf("%d\n", pinNumber);
	int check = level_reg & (1 << pinNumber);
	printf("%d\n", check); //should be 1 or 0
	return check;

}

int buttonCode(GPIO_Handle gpio, int* correctOrder){
	int i = 0;
	//array to store what the order of button presses is
	int buttonOrder[4];
	//sets a variable to get the time to keep track of the time
	time_t startTime = time(NULL);
	//sets a variable to keep track of the time inputted so it isn't exceeded
	int timeLimit = 30;
	//stay in the while loop until the user has pressed the button four times or past the time limit
	while((time(NULL) - startTime) < timeLimit){
		if(i == 4){
			break;
		}
		printf("please push\n");
		usleep(5000);
		//figures out what the current state of all the pins are (5, 6, 7)
		int pin5_state = pinCheck(gpio, 5);
		int pin6_state = pinCheck(gpio, 6);
		int pin7_state = pinCheck(gpio, 7);
		//if the fifth pin is pressed, enters this section where it stays in a while loop until released, then sets the pin number in the button order array before increasing i
		if(pin5_state){
			printf("entered pin5\n");
			while(pin5_state){
				usleep(5000);
				int pin5_state = pinCheck(gpio, 5);
			}
			buttonOrder[i] = 5;
			printf("push 5\n");
			usleep(5000);
			i++;
		}
		//pretty much the same process as for pin5, except for pin6
		else if(pin6_state){
			printf("entered pin6\n");
			while(pin6_state){
				usleep(5000);
				int pin6_state = pinCheck(gpio, 6);
			}
			buttonOrder[i] = 6;
			printf("push 6\n");
			usleep(5000);
			i++;
		}
		//pretty much the same process as for pin5, except for pin7
		else if(pin7_state){
			printf("entered pin7\n");
			while(pin7_state){
				usleep(5000);
				int pin7_state = pinCheck(gpio, 7);
			}
			buttonOrder[i] = 7;
			printf("push 7\n");
			usleep(5000);
			i++;
		}
		//checks if none are being pressed currently just so it isn't caught as an error
		else if(!pin5_state && !pin6_state && !pin7_state);
		//errors return -1
		else{
			//ERROR
			return -1;
		}
	}
	//checks if the button order array and the correct order array match, if they do returns 1, otherwise returns a 0
	printf("Out of the Button Loop\n");
	for(i = 0; i < 4; i++){
		if(buttonOrder[i] != correctOrder[i]){
			return 0;
		}
	}
	return 1;
}

#define LASER1_PIN_NUM 4
#define LASER2_PIN_NUM 9
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber)
{
	if(gpio == NULL)
	{
		return -1;
	}

	if(diodeNumber == 1)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if(level_reg & (1 << LASER1_PIN_NUM))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	if(diodeNumber == 2)
	{
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if(level_reg & (1 << LASER2_PIN_NUM))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return -1;
	}
	
	
}
enum State {START1, GOT_ZERO, GOT_ONE, GOT_KTH_ZERO, GOT_KTH_ONE};

//This function acts as a Hysteresis to account for any bouncing and inconsistencies
int bounceHelper(GPIO_Handle gpio, int diodeNumber) {
	enum State s = START1;
	int input;
	int k = 0;
	//kMax set to 50000 meant to count the number of 1s or 0s and if 50000 in a row, then it is in that state
	int kMax = 50000;
	//sets a variable to get the time to keep track of the time
	time_t startTime = time(NULL);

	//sets a variable to keep track of the time inputted so it isn't exceeded
	float timeOut = 0.25;

	//runs the while loop while grabbing input from the photodiode to and increasing k, unless it reaches the time limit where it will break out
	while ((time(NULL) - startTime) < timeOut){
		//gets input from the photodiode
		input = laserDiodeStatus(gpio, diodeNumber);
		//This state machine works almost the same as Hysteresis, except once zero or one reaches the 50000 threshold, it returns the value 1 or 0
		switch (s){
			case START1:
				k = 0;
				switch (input){
					case 1:
						s = GOT_KTH_ONE;
						k++;
						break;

					case 0:
						s = GOT_KTH_ZERO;
						k++;
						break;
					}
				break;

			case GOT_KTH_ONE:
				switch(input){
					case 1:
						if (k > kMax){
							s = GOT_ONE;
						}
						k++;
						break;
					case 0:
						s = GOT_KTH_ZERO;
						k = 0;
						break;
					}
				break;

			case GOT_KTH_ZERO:
				switch(input){
					case 0:
						if (k > kMax){
							s = GOT_ZERO;
						}
						k++;
						break;
					case 1:
						s = GOT_KTH_ONE;
						k = 0;
						break;
					}
				break;

			case GOT_ONE:
				printf("GOT 1\n");
				return 1;
				break;

			case GOT_ZERO:
				printf("GOT 0\n");
				return 0;
				break;
			default:
				printf("DEFAULT STATE");
				return -1; 
		}
	}
	printf("Timeout\n");
	return 2;
}

enum Sensor{START, DONE, LEFT, RIGHT, BOTH};

int laserCode(GPIO_Handle gpio, int* correctLOrder){

	int laserOrder[4] = {0, 0, 0, 0};
	time_t startTime = time(NULL);
	int timeLimit = 10;
	int i = 0;
	printf("WE IN LASERS\n");

	int counter1 = 0;		
	int counter2 = 0;
	int in = 0;
	int out = 0;

	int firstLight = 0;
	int lastLight = 0;
 

	enum Sensor prevState = START;
	enum Sensor laserSensor = START;

	//while loop for keeping track of the time limit
	while((time(NULL) - startTime) < timeLimit){
		int status1 = 0;
		int status2 = 0;
		//before entering the switch statement, it goes to the Hysteresis to determine whether the diode status is 0 or 1
		status1 = bounceHelper(gpio, 1);
		status2 = bounceHelper(gpio, 2);
		switch(laserSensor){
			//stays in the START state until one of the diodes isn't receiving any light otherwise it stays in the START state, if invalid input then an error message prints out  
			case START:
				if(status1 == 0){
					laserSensor = RIGHT;
					counter1++;
					firstLight = 1;
					printf("RIGHT LASER");
				}
				else if(status2 == 0){
					laserSensor = LEFT;
					counter2++;
					firstLight = 2;
					printf("LEFT LASER");
				}
				else if(status1 == 1);
				else if(status2 == 1);
				else if(status1 == 2);
				else if(status2 == 2);
				else{
					printf("StartERROR\n");
					return -1;
				}
				break;
			//once the state machines enters done, it sees what the previous state was then increases the in or out accordingly. Finally it resets the state to START
			case DONE:
				if(prevState == LEFT && firstLight == 1){
					laserOrder[i] = 1;
					printf("laserOrder %d: 1", i); 
					i++;
					in++;
				}
				else if(prevState == RIGHT && firstLight ==2 ){
					laserOrder[i] = 2;
					printf("laserOrder %d: 2", i); 
					i++;
					out++;
				}
				else if(prevState == LEFT && firstLight == 2){
					laserSensor = START;
				}
				else if(prevState == RIGHT && firstLight == 1){
					laserSensor = START;
				}
				else if(status1 == 2);
				else if(status2 == 2);
				else{
					printf("DoneERROR\n");
					return -1;
				}
				laserSensor = START;
				break;
			//once both lasers aren't receiving input, then the machine enters the both state. Once one of them is receiving light again, then it changes state accordingly
			case BOTH:
				if(status1 == 0 && status2 == 0);
				else if(status1 == 0 && status2 == 1 && prevState == LEFT){
					laserSensor = RIGHT;
					prevState = BOTH;
				}
				else if(status2 == 0 && status1 == 1 && prevState == RIGHT){
					laserSensor = LEFT;
					prevState = BOTH;
				}
				else if(status1 == 0 && status2 == 1 && prevState == RIGHT){
					laserSensor = START;
					counter1--;
				}
				else if(status2 == 0 && status1 == 1 && prevState == LEFT){
					laserSensor = START;
					counter2--;
				}
				else if(status1 == 2);
				else if(status2 == 2);
				else{
					printf("BothERROR\n");
					return -1;
				}
				break;
			//the state machines enters LEFT either if the first light that broke was left, or the first light to recieve light again was the right laser. This allows the machine to determine which was the first and last light to lose laser input
			case LEFT:
				if(status2 == 0 && status1 == 1);
				else if(status1 == 0 && status2 == 0){
					laserSensor = BOTH;
					counter1++;
					prevState = LEFT;
				}
				else if(status1 == 1 && status2 == 1 && prevState == BOTH){
					laserSensor = DONE;
					prevState = LEFT;
				}
				else if(status1 == 1 && status2 == 1 && prevState != BOTH){
					laserSensor = START;
				}
				else if(status1 == 2);
				else if(status2 == 2);
				else{
					printf("LeftERROR\n");
					return -1;
				}
				break;
			//the state machine enters RIGHT when the same things as in LEFT occur, but obviously switch to working with the case RIGHT
			case RIGHT:
				if(status1 == 0 && status2 == 1);
				else if(status2 == 0 && status1 == 0){
					laserSensor = BOTH;
					counter2++;
					prevState = RIGHT;
				}
				else if(status1 == 1 && status2 == 1 && prevState == BOTH){
					laserSensor = DONE;
					prevState = RIGHT;
				}
				else if(status1 == 1 && status2 == 1 && prevState != BOTH){
					laserSensor = START;
				}
				else if(status1 == 2);
				else if(status2 == 2);
				else{
					printf("RIGHTERROR\n");
					return -1;
				}
				break;
			default:
				printf("DefError\n");
				return -1;
				break;
		}
		usleep(5000);
		if(i == 4){
			timeLimit = 0;
		}		
	}
	//prints out the counters before ending the program
	for(i = 0; i < 4; i++){
		if(laserOrder[i] != correctLOrder[i]){
			return 0;
		}
	}
	return 1;
}

void outputOn(GPIO_Handle gpio, int pinNumber)
{
	gpiolib_write_reg(gpio, GPSET(0), 1 << pinNumber);
}

void outputOff(GPIO_Handle gpio, int pinNumber)
{
	gpiolib_write_reg(gpio, GPCLR(0), 1 << pinNumber);
}

void getTime(char* timer){
	struct timeval tv;
	time_t curtime;
	gettimeofday(&tv, NULL);
	curtime = tv.tv_sec;
	strftime(timer, 30, "%m-%d-%Y %T.", localtime(&curtime));
}

void readConfig(FILE* configFile, int* laserCode, int* buttonCode){
	int i = 0;
	int size = 255;
	char codeReader[size];
	*laserCode = 0;
	*buttonCode = 0;
	int code = 0;
	while(fgets(codeReader, size, configFile) != NULL){
		i = 0;
		if(codeReader[i] != '#'){
			if(code == 0){
				while(codeReader[i] != 0){
					if(codeReader[i] <= '9' && codeReader[i] >= '0'){
						*buttonCode *= 10;
						*buttonCode += codeReader[i] - 48;
					}
					i++;
				}
				code++;
			}
			else if(code == 1){
				while(codeReader[i] != 0){
					if(codeReader[i] <= '9' && codeReader[i] >= '0'){
						*laserCode *= 10;
						*laserCode += codeReader[i] - 48;
					}
					i++;
				}
			}
			else{
				printf("ERROR");
			}
		}
		else{
			printf("comment");
		}	
	}
}

int main(){


	//initializes the gpio pins
	GPIO_Handle gpio = initializeGPIO();

	//pin 16 RED LED
	uint32_t sel_reg = gpiolib_read_reg(gpio, GPFSEL(1));
	sel_reg |= 1  << 18;
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);

	//pin 19 GREEN LED lasers
	uint32_t sel_reg_two = gpiolib_read_reg(gpio, GPFSEL(1));
	sel_reg_two |= 1 << 27;
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg_two);

	//Set pin 17 as an output pin for buzzer
	uint32_t sel_reg_three = gpiolib_read_reg(gpio, GPFSEL(1));
	sel_reg_three |= 1  << 21;
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg_three);


	//sets configFile variable to then open and read the file, then closes
	int laser = 0;
	int button = 0;
	FILE* configFile;
	configFile = fopen("/home/pi/LaserButtonCode.cfg", "r");
	readConfig(configFile, &laser, &button);
	fclose(configFile);

	//puts the number obtained from the file into an array to be sent to the functions as the pass code
	int i = 0;
	int correctBOrder[4];
	int correctLOrder[4];
	for(i = 3; i >= 0; i--){
		correctBOrder[i] = (button % 10);
		button = button / 10;
		printf(" %d & %d\n", correctBOrder[i], button);
		correctLOrder[i] = (laser % 10);
		laser = laser / 10;
		printf(" %d & %d\n", correctLOrder[i], laser);
	}

	//sets up the variables for buzzer
	int intrusion = 0;
	int timePassed = 0;

	FILE* logFile2;
	logFile2 = fopen("/home/pi/attemptedIntrusions.log", "a");

	FILE* logFile;
	logFile = fopen("/home/pi/watchdogFile.log", "a");

	//sets up watchdog & ensures default file exists
	int watchdog;
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		return -1;
	}

	//sets watchdog time limit
	int timeout = 15;
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);
	char time1[30];
	getTime(time1);
	//Log that the Watchdog time limit has been set
	fprintf(logFile, "%s : LockBox33.c : The Watchdog time limit has been set\n", time1);
	fflush(logFile);
	
	//The value of timeout will be changed to whatever the current time limit of the
	//watchdog timer is
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);

	int unlocked = 1;

	while(unlocked){
		//Button Code Function
		/*int buttonResult = buttonCode(gpio);
		if(buttonResult == 0){
			outputOn(gpio, 17);
			usleep(100000);
			outputOff(gpio, 17);
			//alarm
		}
		
		if(buttonResult == 1){
			outputOn(gpio, 19);
			usleep(100000);
			outputOff(gpio, 19);
		}

		int laserResult = 0;*/
		//if(buttonResult == 1){
		int laserResult = laserCode(gpio, correctLOrder);
		printf("%d\n", laserResult);
		if(laserResult == 0){
			char time[30];
			getTime(time);
			fprintf(logFile2, "%s : LockBox33.c : Attempted Intrusion\n", time);  
			fflush(logFile2); 
			outputOn(gpio, 16);
			intrusion = 1;
			if(intrusion) {
				while(timePassed < 3000000) { // should loop for 30 seconds
					//Turn on buzzer pin (send voltage)
					gpiolib_write_reg(gpio, GPSET(0), 1 << 17);
					usleep(500);
					gpiolib_write_reg(gpio, GPCLR(0), 1 << 17);
					usleep(500);
					timePassed += 1000;
				}
				timePassed = 0;
				intrusion = 0;
				//Turn the off buzzer pin (stop voltage)
				gpiolib_write_reg(gpio, GPCLR(0), 1 << 17);
			}
			outputOff(gpio, 16);
		}
		else if(laserResult == 1){
			unlocked = 0;
			char time[30];
			getTime(time);
			fprintf(logFile2, "%s : LockBox33.c : LockBox Opened!\n", time);  
			fflush(logFile2); 
			outputOn(gpio, 19);
			usleep(3000000);
			outputOff(gpio, 19);
		}
		//WATCHDOG kick
		//keeps code alive for 15 seconds at a time
		ioctl(watchdog, WDIOC_KEEPALIVE, 0);
		char time[30];
		getTime(time);
		//Log that the Watchdog time limit has been set
		fprintf(logFile, "%s : LockBox33.c : The Watchdog was kicked\n", time);
		fflush(logFile);
	}
	
	//Writing a V to the watchdog file will disable to watchdog and prevent it from
	//resetting the system
	write(watchdog, "V", 1);
	char time2[30];
	getTime(time2);
	//Log that the Watchdog time limit has been set
	fprintf(logFile, "%s : LockBox33.c : The Watchdog was disabled\n", time2);
	fflush(logFile);
	
	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	char time3[30];
	getTime(time3);
	//Log that the Watchdog time limit has been set
	fprintf(logFile, "%s : LockBox33.c : The Watchdog was closed\n", time3);
	fflush(logFile);

	//Free the gpio pins
	gpiolib_free_gpio(gpio);
	char time4[30];
	getTime(time4);
	//Log that the Watchdog time limit has been set
	fprintf(logFile, "%s : LockBox33.c : The GPIO Pins have been freed\n", time4);
	fflush(logFile);

	fclose(logFile);
	fclose(logFile2);
	
	return 0;	
}
