#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SAMPLE 256
#define MAX_CHANNEL 128
#define MAX_NAME 16//change the scanf too
#define COUNT(A) (sizeof(A)/sizeof(A[0]))

typedef struct{
	int width;
	int verbose;
	int round;
}Parameters;

typedef struct{
	char size;
	char name[MAX_NAME];
	char type[MAX_SAMPLE];//U,Z,Data...
	unsigned values[MAX_SAMPLE];
}Channel;

int nb_timestamps=0;
long long unsigned timestamps[MAX_SAMPLE];
Channel channels[MAX_CHANNEL];

void parseInst(FILE*f){
	char data[256];
	int c,data_pos=0;

	while((c=fgetc(f))!=EOF){
		if(c=='$'){//maybe it's a $end ?
			int pos=ftell(f);//save the pos just after the $ (in case it's not a $end)
			if((fgetc(f)=='e')&&(fgetc(f)=='n')&&(fgetc(f)=='d')&&(!isalpha(fgetc(f)))){
				data[data_pos]='\0';
				
				//printf("instr:%s\n",data);
				if(strstr(data,"var reg")){//new register !
					int size;char name[MAX_NAME],id;
					if(sscanf(data,"var reg %i %c %15s",&size,&id,&name)==3){
						channels[id].size=size;
						memcpy(channels[id].name,name,sizeof(name)-1);
						//printf(">>>>size:%i id:%c name:%s\n",size,id,name);
					}
				}
				return;
			}else{
				fseek(f,pos,SEEK_SET);
			}
		}
		if(data_pos<sizeof(data)-1)
			data[data_pos++]=c;
	}
}

void parseTime(FILE*f){
	long long unsigned stamp=0;
	int i;char c;
	while((c=fgetc(f))!=EOF){
		if(!isdigit(c)){
			ungetc(c,f);
			if(nb_timestamps<COUNT(timestamps)){
				if(nb_timestamps>0){//copy all previous channels_size values to the current one
					for(i=0;i<COUNT(channels);i++){
						channels[i].values[nb_timestamps]=channels[i].values[nb_timestamps-1];
						channels[i].type  [nb_timestamps]=channels[i].type  [nb_timestamps-1];
					}
				}
				timestamps[nb_timestamps]=stamp;
				nb_timestamps++;
			}
			//printf("time:%llu\n",stamp);
			return;
		}
		stamp=stamp*10+(c-'0');
	}
}

void parseData(FILE*f){
	unsigned data=0,base=10;
	char type,c,id='?';
	while((c=fgetc(f))!=EOF){
		if(isdigit(c))
			data=data*base+(c-'0');
		else if(c=='b')base=2;
		else if(c=='x')base=16;
		else if(c=='Z')type='Z';//undefined type
		else if(c=='U')type='U';//undefined type
		else if(c==' ');
		else{//unexpected character = end ?
			id=c;
			if(!nb_timestamps)
				return printf("timestamps unpreceded data\n");
			if(!channels[id].size)
				return ;//printf("undeclared channel (%c)\n",id);
			channels[id].values[nb_timestamps-1]=data;
			return;
		}
	}
}

void parseFile(FILE*f){
	int c;
	while((c=fgetc(f))!=EOF){
		if(isspace(c))continue;
		if(isdigit(c) || c=='b' || c=='Z' || c=='U'){
			ungetc(c,f);
			parseData(f);
		}else if(c=='$'){
			parseInst(f);
		}else if(c=='#'){
			parseTime(f);
		}else{
			printf("unknow char : %c\n",c);
		}
	}
}
void showVertical(Parameters*params){
	int chan,smpl,w;
	for(chan=0;chan<COUNT(channels);chan++){
		if(!channels[chan].size)continue;//skip empty channels
		if(params->width>=2)printf("\n");
		printf("% 10s(%c)[%2i]: ",channels[chan].name,chan,channels[chan].size);
		for(smpl=0;smpl<COUNT(timestamps) && timestamps[smpl]!=~0 ;smpl++){
			char type=channels[chan].type[smpl];
			unsigned data=channels[chan].values[smpl];
			if(channels[chan].size==1){//binary
				w=params->width;
				if(params->round && smpl>0 && !channels[chan].type[smpl-1]){//have a previous data => can print a transition
					if(channels[chan].values[smpl]!=channels[chan].values[smpl-1]){//the value changed
						printf(params->round==2?"%c":"|",(channels[chan].values[smpl-1]?'\\':'/'));//from H to L or L to H ?
						w-=1;
					}
				}
				while(w-->0)printf("%c",type?type:(data?238:95));
			}else{//bus
				if(channels[chan].type[smpl])//not a data
					printf("%c",params->width,channels[chan].type[smpl]);
				else
					printf("%*X",params->width,channels[chan].values[smpl]);
			}
		}
		printf("\n");
	}
}
void showHelp(char*arg0,Parameters*p){
	printf("Usage: %s [FILE] [OPTION]...:\n"
				" -h	: display this help screen\n"
				" -v=%i	: verbose level (0:fatal,1:error,2:warning,3:debug)\n"
				" -w=%i	: sample ascii width\n"
				" -r=%i	: rounded wave (0:none,1:pipe,2:slash)\n"
				,arg0,p->verbose,p->width,p->round);
}
int main(int argc,char**argv){
	//init context
	memset(channels,0,sizeof(channels));
	memset(timestamps,~0,sizeof(timestamps));
	
	Parameters params={
		.verbose=0,
		.width=2,
		.round=2,
	};
	// load arguments
	FILE*f=((argc>1)&&(argv[1][0]!='-'))?fopen(argv[1],"r"):stdin;
	if(!f)return printf("unable to open %s\n",argv[1]);
	int i;
	for(i=1;i<argc;i++){//parse "-" arguments
		if(argv[i][0]!='-')continue;
		switch(argv[i][1]){
			case 'h':showHelp(argv[0],&params);return 0;break;
			case 'w':params.width=atoi(argv[i]+3);break;
			case 'r':params.round=atoi(argv[i]+3);break;
			default:printf("skiped param '%c'",argv[i][1]);
		}
	}

	parseFile(f);
	showVertical(&params);
	
	fclose(f);
	return 0;
}