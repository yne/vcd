#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SAMPLE 256
#define MAX_CHANNEL 128
#define MAX_NAME 32//change the scanf too
#define COUNT(A) (sizeof(A)/sizeof((A)[0]))

typedef struct{
	int width;
	int verbose;
	int round;
	char*scope;
	FILE*fin;
	FILE*fout;
}Parameters;

typedef struct{
	int  size;
	int  scope;
	char name[MAX_NAME];
	char    type[MAX_SAMPLE];//'U','Z','\0'=Data
	unsigned val[MAX_SAMPLE];
}Channel;

typedef struct{
	long long unsigned timestamps[MAX_SAMPLE];
	Channel ch[MAX_CHANNEL];
	char scopes[32][32];//[0]=root
	int nb,nb_scopes;
	char date[32],version[32],scale[32];//file info
	//parsing related values
	int cur_scopes;
}Parser;

void showHelp(char*arg0,Parameters*p){
	fprintf(stderr,"Usage: %s [FILE] [OPTION]...:\n"
				" -h	: display this help screen\n"
				" -v=%i	: verbose level (0:fatal,1:error,2:warning,3:debug)\n"
				" -w=%i	: sample ascii width\n"
				" -r=%i	: rounded wave (0:none,1:pipe,2:slash)\n"
				" -i=file	: input file\n"
				" -s=a,b,c	: scope(s) to display\n"
				,arg0,p->verbose,p->width,p->round);
}

void parseArgs(int argc,char**argv,Parameters*params){
	int i;
	for(i=1;i<argc;i++){//parse "-" arguments
		if(argv[i][0]!='-'){
			params->fin=fopen(argv[i],"r");
		}else{
			switch(argv[i][1]){
				case 'i':params->fin=fopen(argv[i]+3,"r");break;
				case 'o':params->fout=fopen(argv[i]+3,"w+");break;
				case 'h':showHelp(argv[0],params);exit(0);break;
				case 'w':params->width=atoi(argv[i]+3);break;
				case 'r':params->round=atoi(argv[i]+3);break;
				case 's':params->scope=argv[i]+3;break;
				default:fprintf(stderr,"unknow param '%c'",argv[i][1]);
			}
		}
	}
}

void parseInst(Parameters*params,Parser*p){
	char token[32];
	fscanf(params->fin,"%31s",token);
	//printf("%s\n",token);
	if(!strcmp("var",token)){
		int id=0;
		Channel tmp={};
		fscanf(params->fin," reg %d %c %31[^ $]",&(tmp.size),(char*)&id,tmp.name);//space in name allowed ?
		p->ch[id]=tmp;//printf("size=%i <%c> name=<%s>\n",size,id,data);
		p->ch[id].scope=p->cur_scopes;
	}
	else if(!strcmp("scope",token))         {fscanf(params->fin,"%*127s %127[^ $]",p->scopes[p->cur_scopes=++(p->nb_scopes)]);}
	else if(!strcmp("date",token))          {fscanf(params->fin,"\n%31[^$\n]",p->date);}
	else if(!strcmp("version",token))       {fscanf(params->fin,"\n%31[^$\n]",p->version);}
	else if(!strcmp("timescale",token))     {fscanf(params->fin,"\n%127[^$\n]",p->scale);}
	else if(!strcmp("comment",token))       {fscanf(params->fin,"\n%*[^$]");}
	else if(!strcmp("upscope",token))       {fscanf(params->fin,"\n%*[^$]");p->cur_scopes=0;}/*back to root */
	else if(!strcmp("enddefinitions",token)){fscanf(params->fin,"\n%*[^$]");}
	else if(!strcmp("end",token)){}
	else {printf("unknow token : %s\n",token);}
}

void parseTime(Parameters*params,Parser*p){
	long long unsigned stamp=0;
	int i;char c;
	while((c=fgetc(params->fin))!=EOF){
		if(isdigit(c)){
			stamp=stamp*10+(c-'0');
		}else{
			ungetc(c,params->fin);
			if(p->nb >= COUNT(p->timestamps))return;
			if(p->nb > 0){//copy all previous channels_size val to the current one
				for(i=0;i<COUNT(p->ch);i++){
					p->ch[i].val [p->nb]=p->ch[i].val [p->nb-1];
					p->ch[i].type[p->nb]=p->ch[i].type[p->nb-1];
				}
			}
			p->timestamps[p->nb]=stamp;
			p->nb++;
			return;
		}
	}
}

void parseData(Parameters*params,Parser*p){
	unsigned data=0,base=10;
	char type='\0';
	int id=0,c=fgetc(params->fin);
	if(c=='b'){//parsing bus data b%[0-9UZ]+ %c
		base=2;
		while((c=fgetc(params->fin))!=EOF && c!=' '){
			if(isdigit(c))data=data*base+(c-'0');
			else type=c;//letter (Z,U,...) = undefined type, but we don't know the id yet
		}
		id=fgetc(params->fin);
		p->ch[id].type[p->nb-1]=type;
		p->ch[id].val [p->nb-1]=data;
	}else{//parsing state %[0-9UZ] %c
		id=fgetc(params->fin);
		if(isalpha(c))p->ch[id].type[p->nb-1]=c;
		if(isdigit(c))p->ch[id].val [p->nb-1]=c-'0';
	}
}

void parseFile(Parameters*params,Parser*p){
	int c;
	while((c=fgetc(params->fin))!=EOF){
//		printf("?%c\n",c);
		if(isspace(c))continue;
		if(c=='$'){
			parseInst(params,p);
		}else if(isdigit(c) || c=='b' || c=='Z' || c=='U'){
			ungetc(c,params->fin);
			parseData(params,p);
		}else if(c=='#'){
			parseTime(params,p);
		}else{
			fprintf(stderr,"unknow char : %c\n",c);
		}
	}
}

void showVertical(Parameters*params,Parser*p){
	int chan,smpl,w;
	
	if(p->nb      )fprintf(params->fout,"%i samples",p->nb);
	if(p->date [0])fprintf(params->fout," / %s",p->date);
	if(p->scale[0])fprintf(params->fout," / %s",p->scale);
	fprintf(params->fout,"\n");
	
	for(chan=0;chan < COUNT(p->ch);chan++){
		if(!p->ch[chan].size)continue;//skip empty ch
		if(params->scope && (!p->ch[chan].scope || !strstr(params->scope,p->scopes[p->ch[chan].scope])))
			continue;//skip root node or unrelated node if scope-only wanted
		
		
		if((!chan && p->ch[chan].scope) || (chan>0 && (p->ch[chan].scope!=p->ch[chan-1].scope)))
			fprintf(params->fout,"+-- %s\n",p->ch[chan].scope?p->scopes[p->ch[chan].scope]:"");
		
		fprintf(params->fout,"%c %10s(%c)[%2i]: ",p->ch[chan].scope?'|':' ',p->ch[chan].name,chan,p->ch[chan].size);
		for(smpl=0;smpl < p->nb ;smpl++){
			char     type = p->ch[chan].type[smpl];
			unsigned data = p->ch[chan].val [smpl];
			if(p->ch[chan].size==1){//binary
				w=params->width;
				//have a previous data => can print a transition
				if(params->round && smpl>0 && !p->ch[chan].type[smpl-1]){
					if(p->ch[chan].val[smpl]!=p->ch[chan].val[smpl-1]){//the value changed
						//from H to L or L to H ?
						fprintf(params->fout,params->round==2?"%c":"|",(p->ch[chan].val[smpl-1]?'\\':'/'));
						w-=1;
					}
				}
				while(w-->0)fprintf(params->fout,"%c",type?type:(data?/*238*/'-':95));
			}else{//bus
				if(p->ch[chan].type[smpl])//not a data
					fprintf(params->fout,"%*c",params->width,p->ch[chan].type[smpl]);
				else
					fprintf(params->fout,"%*X",params->width,p->ch[chan].val[smpl]);
			}
		}
		fprintf(params->fout,"%c%c\n",params->width>=2?'\n':' ',p->ch[chan].scope && params->width>=2?'|':' ');
	}
}

int main(int argc,char**argv){
	Parameters params={
		.verbose=0,
		.width=2,
		.round=2,
		.fin=stdin,
		.fout=stdout,
	};
	Parser data;
	memset(&data,0,sizeof(Parser));

	parseArgs(argc,argv,&params);
	if(params.fin){
		parseFile(&params,&data);
		showVertical(&params,&data);
	}else{
		fprintf(stderr,"no input stream\n");
	}
	
	fclose(params.fin);
	
	return 0;
}
