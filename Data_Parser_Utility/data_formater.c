//-------------------------------------------------------------------------------------------------------------------------------------------------------------
// UMSATS Rocketry 2019/2020
//
// Repository:
//  UMSATS/Avionics-Tools
//
// File Description:
//  Source file for the data formater. This converts a log file from the flight computer into a csv file.
//
// History
// 2019-04 Joseph Howarth
// - Created.


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


#define LOG_NAME  "UMSATS_ROCKET.log"	//This is the input file name.
#define OUTPUT_NAME "flightComputer.csv"

#define ACC_TYPE 			0x800000
#define GYRO_TYPE			0x400000
#define PRES_TYPE			0x200000
#define	TEMP_TYPE			0x100000

#define DROGUE_DETECT		0x080000
#define DROGUE_DEPLOY		0x040000

#define MAIN_DETECT			0x020000
#define MAIN_DEPLOY			0x010000

#define LAUNCH_DETECT		0x008000
#define LAND_DETECT			0x004000

#define POWER_FAIL			0x002000
#define	OVERCURRENT_EVENT	0x001000

#define	ACC_LENGTH	6		//Length of a accelerometer measurement in bytes.
#define	GYRO_LENGTH	6		//Length of a gyroscope measurement in bytes.
#define	PRES_LENGTH	3		//Length of a pressure measurement in bytes.
#define	TEMP_LENGTH	3		//Length of a temperature measurement in bytes.
#define ALT_LENGTH  4		//Length of a altitude measurement in bytes.
#define HEADER_SIZE 3		//Length of the header(type,flags,timestamp) in bytes.

#define ACC_OFFSET  0		//Offset of the acceleration data within 1 packet, in bytes.
#define GYRO_OFFSET 6		//Offset of the gyroscope data within 1 packet, in bytes.
#define PRES_OFFSET 12		//Offset of the pressure data within 1 packet, in bytes.
#define TEMP_OFFSET 15		//Offset of the temperature data within 1 packet, in bytes.
#define ALT_OFFSET  18		//Offset of the altitude data within 1 packet, in bytes.

typedef struct{

	int16_t x;
	int16_t y;
	int16_t z;
	uint16_t time;

} acc_measure;

typedef struct{

	int16_t x;
	int16_t y;
	int16_t z;
	uint16_t time;

} gyro_measure;

typedef struct{

	uint32_t pres;
	uint16_t time;

} pres_measure;

typedef struct{

	int32_t temp;
	uint16_t time;

} temp_measure;

typedef union {

	uint32_t byte_val;
	float	 float_val;
	
} alt_measure;

typedef struct{

	uint8_t header1;
	uint8_t header2;
	uint8_t header3;
	uint8_t data[22];

}measure;

void printArray(char arr[], int n){

	int i;
	for(i=0;i<n;i++){

		printf("%x ",arr[i]);
	}
	printf("\n");
}

int main(){

	FILE *fp;
	FILE *fp_out;

	char buffer[100];
	uint8_t m_buff[25];

	measure m;

	//For tracking the number of measurements read,total bytes read, and number of long/short measuerements.
	int m_count =0;
	uint32_t bytesRead=0;
	int numLong=0;
	int numShort=0;

	//Open the log file and output csv file.
	fp = fopen(LOG_NAME,"rb");
	if(fp == NULL){
		printf("Could not open log file.:%s\n",LOG_NAME);
		return -1;
	}
	
	fp_out = fopen(OUTPUT_NAME,"wb");
	if(fp_out == NULL){
		printf("Could not open log file:%s.\n",OUTPUT_NAME);
		return -1;
	}



	//Skip the first two lines.
	//Due to how the serial data is saved to log, the read command
	//and message are saved in log and should be ignored.
	fgets(buffer,100,fp);
	printf("Skipped: %s\n",buffer); 
	fgets(buffer,100,fp);
	printf("Skipped: %s\n",buffer);


	//Skip any null characters, so we start reading from begining of data. Data never starts with a null char.
	uint8_t  n_char = '\0';
	int count=0;
	while(n_char == '\0'){
		n_char = fgetc(fp);
		count++;
	}
	printf("Skiped %d null chars.\n",count);
	fseek(fp,-1,SEEK_CUR);


	//This skips to the start of the proper data. The start address was found manually by inspection. 
	int start_address = 876;
	fseek(fp,0,SEEK_SET);
	while(start_address>0){
		n_char = fgetc(fp);
		start_address--;
	}

	//Initialize all variables.
	uint32_t prevTime =0;		//This variable is used to keep track of absolute time, since only the delta t is included in data.
	int done = 0;				//Flag for when were done reading the data.
	int stat = 0;				//Status variable for reads from file etc.

	uint8_t event_occur = 0;
	pres_measure p;
	temp_measure t;
	acc_measure a;
	gyro_measure g;
	alt_measure alt;

	p.pres = 0;
	p.time = 0;
	t.temp = 0;
	t.time = 0;

	a.time = 0;
	a.x = 0;
	a.y = 0;
	a.z = 0;

	g.time = 0;
	g.x = 0;
	g.y = 0;
	g.z = 0;
	alt.float_val = 0.0;


	uint32_t header_whole = 0;	//Used for storing the header as a single value.

	uint32_t running_count = 0; 
	uint32_t buff_sz = 256;

	while(!done){

		//Read a header (3 bytes).
		stat = fread(&m,1,3,fp);
		if(stat != 3){
			done =1;
			break;
		}

		//Check if header is valid. If not stop.
		if ((m.header1 & 0xF0) == 0) {
			printf("Stopping because of bad header.");
			//If the header's type is 0, that would mean that no data is included in measeurement.
			//This should never happen.
			break;
		}
		if (((m.header1 & 0xF0) !=(0xC0))&&((m.header1 & 0xF0) != (0xF0))) {
			printf("Stopping because of bad header.");
			break;
		} 

		uint8_t length =0;	//Length of the measurement.
		m_count ++;


		header_whole = (m.header1 << 16) + (m.header2 << 8) + (m.header3);
		printf("%d Header value: %x\n",m_count,header_whole);

		//Determine how many bytes to read as part of the current measurement.
		if(header_whole& ACC_TYPE){
			printf("This measurement contains accelerometer data.\n");
			length +=ACC_LENGTH;

		}
		if(header_whole& GYRO_TYPE){
			printf("This measurement contains gyroscope data.\n");
			length +=GYRO_LENGTH;
		}
		if(header_whole& PRES_TYPE){
			printf("This measurement contains pressure data.\n");
			length+=PRES_LENGTH+ALT_LENGTH;
		}
		if(header_whole& TEMP_TYPE){
			printf("This measurement contains temperature data.\n");
			length+=TEMP_LENGTH;
			numLong++;
		}else{
			numShort++;
		}

		running_count +=length;


		stat = fread(&m.data,1,length,fp);
		if(stat != length){

			done =1;
			break;
		}


		bytesRead += 3+length; 
		printArray(m.data,length); 

		uint32_t time_delta = header_whole &(0x0FFF);
		uint32_t time_abs = time_delta+prevTime;
		printf("At time t=%d: \n",time_delta+prevTime);

		prevTime += time_delta;

		if(header_whole & ACC_TYPE){

			a.x = (m.data[0+ACC_OFFSET]<<8) + m.data[1+ACC_OFFSET];
			a.y = (m.data[2+ACC_OFFSET]<<8) + m.data[3+ACC_OFFSET];
			a.z = (m.data[4+ACC_OFFSET]<<8) + m.data[5+ACC_OFFSET];


			printf("A.x = %d A.y = %d A.z = %d \n",a.x,a.y,a.z);
		}
		if(header_whole& GYRO_TYPE){

			g.x = (m.data[0+GYRO_OFFSET]<<8) + m.data[1+GYRO_OFFSET];
			g.y = (m.data[2+GYRO_OFFSET]<<8) + m.data[3+GYRO_OFFSET];
			g.z = (m.data[4+GYRO_OFFSET]<<8) + m.data[5+GYRO_OFFSET];

			printf("G.x = %d G.y = %d G.z = %d \n",g.x,g.y,g.z);
		}
		if(header_whole& PRES_TYPE){

			p.pres = (m.data[0+PRES_OFFSET]<<16) + (m.data[1+PRES_OFFSET]<<8) + m.data[2+PRES_OFFSET];

			printf("P:%d\n",p.pres);
		}
		if(header_whole& TEMP_TYPE){

			t.temp = (m.data[0+TEMP_OFFSET]<<16) + (m.data[1+TEMP_OFFSET]<<8) + m.data[2+TEMP_OFFSET];

			printf("T:%d\n",t.temp);

			alt.byte_val = (m.data[0 + ALT_OFFSET] << 24) + (m.data[1 + ALT_OFFSET] << 16) + (m.data[2 + ALT_OFFSET] << 8) + (m.data[3 + ALT_OFFSET]);
		}


		if (header_whole & DROGUE_DETECT) {
			event_occur += (DROGUE_DETECT >> 12);
		}
		if (header_whole & DROGUE_DEPLOY) {
			event_occur += (DROGUE_DEPLOY >> 12);
		}
		if (header_whole & MAIN_DETECT) {
			event_occur += (MAIN_DETECT >> 12);
		}
		if (header_whole & MAIN_DEPLOY) {
			event_occur += (MAIN_DEPLOY >> 12);
		}
		if (header_whole & LAUNCH_DETECT) {
			event_occur += (LAUNCH_DETECT >> 12);
		}
		if (header_whole & LAND_DETECT) {
			event_occur += (LAND_DETECT >> 12);
		}
		if (header_whole & POWER_FAIL) {
			event_occur += (POWER_FAIL >> 12);
		}
		if (header_whole & OVERCURRENT_EVENT) {
			event_occur += (OVERCURRENT_EVENT >> 12);
		}
		

		char str[150];
		sprintf(str,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%d\n",time_abs,a.x,a.y,a.z,g.x,g.y,g.z,p.pres,t.temp,alt.float_val,event_occur);

		fputs(str,fp_out);

	}
	fclose(fp);
	fclose(fp_out);
	printf("Num of long: %d num of short: %d\n",numLong,numShort) ;                                  
	printf("Bytes read: %d",bytesRead);

	return 0;
}
