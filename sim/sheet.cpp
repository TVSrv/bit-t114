// Лист всех форм — для проверки рисунка
#include "../core/art.h"
#include <cstdio>
using namespace pet;
static gfx::Color fb[gfx::W*gfx::H];
int main(int argc,char**argv){
  gfx::Canvas c(fb);
  for(int page=0;page<3;page++){
    c.clear(pal::editor);
    for(int i=0;i<4;i++){
      int f=page*4+i; if(f>=FORM_COUNT) break;
      Pose p; p.x=30+i*60; p.y=100; p.t=1234+i*300;
      p.mouth=(Mouth)(i*2%9); p.eyes=(Eyes)(page==2?i:0);
      drawPet(c,(Form)f,p);
      c.text(gfx::font_small,30+i*60,110,formName((Form)f),pal::fg,255,gfx::CENTER);
    }
    char n[64]; snprintf(n,64,"%s/sheet%d.ppm",argv[1],page);
    FILE*o=fopen(n,"wb"); fprintf(o,"P6 240 135 255\n");
    for(int k=0;k<gfx::W*gfx::H;k++){gfx::Color v=fb[k];unsigned char px[3]={(unsigned char)((v>>11)<<3),(unsigned char)(((v>>5)&63)<<2),(unsigned char)((v&31)<<3)};fwrite(px,1,3,o);}
    fclose(o);
  }
}
