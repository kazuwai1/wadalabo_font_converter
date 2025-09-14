#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#ifdef MSDOS
#define WB "wb"
#else
#define WB "w"
#endif

#define CODE_SIZE 4418

#define DEFAULTTHRESH 10.0
double thresh=DEFAULTTHRESH;
struct {
  char *str;
  int len;
} allkanji[94*94];
char *vfbase="ZFont";

void usage(char *cmd)
{
  fprintf(stderr,"usage: %s [-thresh threshold] [-base basename] [file...]\n",cmd);
  fprintf(stderr," -thresh .threshold     default %f\n",DEFAULTTHRESH);
  fprintf(stderr," -base .baesname        default %s\n","ZFont");
  exit(1);
}

int hex1(int x)
{
 return (x>='a'?x-'a'+10:(x>='A'?x-'A'+10:x-'0'));
}

char *allocbin(char *buf, int len)
{
  int i;
  char *cptr;

  if((cptr=malloc(len))==NULL){
    fprintf(stderr,"Failed in malloc()\n");
    exit(1);
  }
  for(i=0;i<len;i++)
    cptr[i]=(hex1(buf[i*2])<<4)|hex1(buf[i*2+1]);
  return cptr;
}

void parsekanji(char *filename)
{
  FILE *fd;
  int len,kcode,koffset,klen;
  char buf[4096];

  if((fd=fopen(filename,"r"))==NULL){
    fprintf(stderr,"File %s is not found\n",filename);
    exit(1);
  }
  while(fgets(buf,4096,fd)!=NULL){
    len=strlen(buf);
    if(buf[0]=='<' && !strncmp(buf+len-6 ,"CompD",5)){
      kcode=strtol(buf+len-12,NULL,16);
      koffset=(((kcode>>8)&255)-0x21)*94+((kcode&255)-0x21);
      klen=(len-16)/2;
      allkanji[koffset].str=allocbin(buf+1,klen);
      allkanji[koffset].len=klen;
    }
  }
  fclose(fd);
}

void output_short(FILE *fd, int val)
{
  fputc(val,fd);
  fputc(val>>8,fd);
}

void output_long(FILE *fd, int val)
{
  fputc(val,fd);
  fputc(val>>8,fd);
  fputc(val>>16,fd);
  fputc(val>>24,fd);
}

int offsets[CODE_SIZE];
void output_zeit(int level)
{
  char filename[256];
  FILE *fd;
  int kbase,klimit,offset,i;
  long last;

  sprintf(filename,"%s.vf%d",vfbase,level);
  if((fd=fopen(filename,WB))==NULL){
    fprintf(stderr,"Failed opening file %s\n",filename);
    exit(1);
  }
  fseek(fd,(long)(CODE_SIZE*4+2),0);
  if(level==1){
    kbase=0;
    klimit=0x5e*(0x50-0x21);
  }
  else {
    kbase=0x5e*(0x50-0x21);
    klimit=0x5e*(0x75-0x50);
  }
  for(offset=i=0;i<klimit;i++){
    if(allkanji[i+kbase].len>0){
      offsets[i]=offset;
      offset+=zeit_character(fd,allkanji[kbase+i].str,allkanji[kbase+i].len);
    }
    else offsets[i]= 0xffffffff;
  }
  last=ftell(fd);
  fseek(fd,0L,0);
  output_short(fd,level);
  for(i=0;i<CODE_SIZE;i++)
    output_long(fd,offsets[i]);
  fseek(fd,last,0);
  fclose(fd);
}

int rest;
int outbuf;
int count;

void init_10()
{
  rest=0;
  count=0;
}

int output_10(FILE *fd, int val)
{
  outbuf<<=10;
  outbuf|=(val&0x3ff);
  rest+=10;
  if(rest>=16){
    rest-=16;
    fputc(outbuf>>rest,fd);
    fputc(outbuf>>(rest+8),fd);
    count+=2;
  }
}

int output_x(FILE *fd, int val)
{
  if(val<0) val=0;
  else if(val>1022) val=1022;
  return output_10(fd,val);
}

int output_y(FILE *fd, int val)
{
  val=1000-val;
  if(val<0) val=0;
  else if(val>1022) val=1022;
  return output_10(fd,val);
}

int flush_10(FILE *fd)
{
  if(rest>0){
    if(rest<6)output_10(fd,0);
    output_10(fd,0);
  }
  return(count);
}

void decrypt_str(unsigned char *cipher, unsigned char *plain, int len)
{
  unsigned short r=4330,c1=52845,c2=22719;

  while(--len>=0){
    *plain++=(*cipher ^ (r>>8));
    r=(*cipher++ +r)*c1+c2;
  }
}

#define STACKSIZE 1024
int stack[STACKSIZE];
#define PUSH(x) (*--sp=(x))
#define DISCARD(n) (sp+=n)

struct point {
  int tag,x,y;
} points[1024];  
/* tags */
#define NONE 0
#define LINE 1
#define BEZIER 2

void output_bezier(FILE *fd, double x0,double y0,double x1,double y1,double x2,double y2,double x3,double y3)
{
  double minx,miny,maxx,maxy,tempx,tempy;
  minx=(x0>x1?x1:x0);
  minx=(x2>minx?minx:x2);
  minx=(x3>minx?minx:x3);
  miny=(y0>y1?y1:y0);
  miny=(y2>miny?miny:y2);
  miny=(y3>miny?miny:y3);
  maxx=(x0<x1?x1:x0);
  maxx=(x2<maxx?maxx:x2);
  maxx=(x3<maxx?maxx:x3);
  maxy=(y0<y1?y1:y0);
  maxy=(y2<maxy?maxy:y2);
  maxy=(y3<maxy?maxy:y3);
  if(maxx-minx<thresh || maxy-miny<thresh){
    output_x(fd,(int)(x3+0.5));
    output_y(fd,(int)(y3+0.5));
    return;
  }
  else{
    tempx=0.125*x0+0.375*x1+0.375*x2+0.125*x3;
    tempy=0.125*y0+0.375*y1+0.375*y2+0.125*y3;
    output_bezier(fd,x0,y0,
		  0.5*x0+0.5*x1, 0.5*y0+0.5*y1,
		  0.25*x0+0.5*x1+0.25*x2, 0.25*y0+0.5*y1+0.25*y2,
		  tempx,tempy);
    output_bezier(fd,tempx,tempy,
		  0.25*x1+0.5*x2+0.25*x3, 0.25*y1+0.5*y2+0.25*y3,
		  0.5*x2+0.5*x3, 0.5*y2+0.5*y3,
		  x3,y3);
  }
}

void flush_point(FILE *fd,int p)
{
  int i;

  if(p>0){
    output_x(fd,points[0].x);
    output_y(fd,points[0].y);
    for(i=1;i<p;i++){
      switch (points[i].tag){
      case BEZIER:
	output_bezier(fd,
		      (double)points[i-1].x,(double)points[i-1].y,
		      (double)points[i].x,(double)points[i].y,
		      (double)points[i+1].x,(double)points[i+1].y,
		      (double)points[i+2].x,(double)points[i+2].y);
	i+=2;
	break;
      case LINE:
	output_x(fd,points[i].x);
	output_y(fd,points[i].y);
      }
    }
    output_10(fd,0x3ff);
    output_10(fd,0x3ff);
  }
}

void mf_charstr(FILE *fd, unsigned char *plain, int len)
{
  unsigned char *limit=plain+len,uc;
  int n;
  int *sp=stack+STACKSIZE;
  int x=0,y=0,p=0;

  while(plain<limit){
    uc= *plain++;
    if(32 <=uc && uc<=246)
      PUSH((int)uc-139);
    else if(247<=uc && uc<=250)
      PUSH(((int)uc-247)*256+(*plain++)+108);
    else if(251<=uc && uc<=254)
      PUSH(-((int)uc-251)*256-(*plain++)-108);
    else if(uc==255){
      PUSH((*plain<<24)|(*(plain+1)<<16)|(*(plain+2)<<8)|(*(plain+3)));
      plain+=4;
    }
    else
      switch (uc){
      case 14:/* printf("endchar\n"); */
	flush_point(fd,p);p=0;
	break;
      case 13:/* printf("hsbw\n"); */
	DISCARD(2);
	break;
      case 12:
	switch(*plain++){
	case 6:/* printf("seac\n"); */
	  DISCARD(5);
	  break;
	case 7:/* printf("sbw\n"); */
	  DISCARD(4);
	  break;
	case 0: /* printf("dotsection\n"); */
	  break;
	case 2:/* printf("hstem3\n"); */
	  DISCARD(6);
	  break;
	case 1:/* printf("vstem3\n"); */
	  DISCARD(6);
	  break;
	case 12:/* printf("div\n"); */
	  DISCARD(2);
	  break;
	case 16:/* printf("callothersubr\n"); */
	  DISCARD(sp[1]+2);
	  break;
	case 17:/* printf("pop\n"); */
	  DISCARD(1);
	  break;
	case 33:/* printf("setcurrentpoint\n"); */
	  DISCARD(2);
	  break;
	}
	break;
      case 9: /* printf("closepath\n"); */
	flush_point(fd,p);p=0;
	break;
      case 6: /* printf("hlineto\n"); */
	x+= *sp++;
	points[p].tag=LINE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 22: /* printf("hmoveto\n"); */
	x+= *sp++;
	flush_point(fd,p);p=0;
	points[p].tag=NONE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 31:/* printf("hvcurveto\n"); */
	x+= sp[3];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	x+=sp[2];y+=sp[1];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	y+=sp[0];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	DISCARD(4);
	break;
      case 5:/*	printf("rlineto\n"); */
	y+= *sp++;x+= *sp++;
	points[p].tag=LINE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 21:/* printf("rmoveto\n"); */
	y+= *sp++;x+= *sp++;
	flush_point(fd,p);p=0;
	points[p].tag=NONE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 8:/*	printf("rrcurveto\n"); */
	x+= sp[5];y+=sp[4];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	x+=sp[3];y+=sp[2];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	x+=sp[1];y+=sp[0];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	DISCARD(6);
	break;
      case 30:/* printf("vhcurveto\n"); */
	y+=sp[3];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	x+=sp[2];y+=sp[1];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	x+=sp[0];
	points[p].tag=BEZIER;
	points[p].x=x;
	points[p++].y=y;
	DISCARD(4);
	break;
      case 7: /* printf("vlineto\n"); */
	y+= *sp++;
	points[p].tag=LINE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 4:/* printf("vmoveto\n"); */
	y+= *sp++;
	flush_point(fd,p);p=0;
	points[p].tag=NONE;
	points[p].x=x;
	points[p++].y=y;
	break;
      case 1:/* printf("hstem\n"); */
	DISCARD(2);
	break;
      case 3:/*	printf("vstem\n"); */
	DISCARD(2);
	break;
      case 10:/* printf("callsubr\n"); */
	DISCARD(1);
	break;
      case 11:/* printf("return\n"); */
	break;
      }
  }
  output_10(fd,0x3ff);
  output_10(fd,0x3ff);
}

int zeit_character(FILE *fd, char *str, int len)
{
  unsigned char plain[4096*sizeof(int)];

  decrypt_str(str,plain,len);
  init_10();
  mf_charstr(fd,plain,len);
  return flush_10(fd);
}

int main(int ac, char *ag[])
{
  int i;
  for(i=1;i<ac;i++){
    if(*ag[i]=='-'){
      if(!strcmp(ag[i]+1,"thresh"))
	thresh=strtod(ag[++i],0);
      else if(!strcmp(ag[i]+1,"base"))
	vfbase=ag[++i];
      else usage(ag[0]);
    }
    else{
      parsekanji(ag[i]);
    }
  }
  output_zeit(1);
  output_zeit(2);
  exit(0);
}
