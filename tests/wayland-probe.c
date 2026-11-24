/* wayland-probe: from inside the box, tries to use the clipboard without focus (ext-data-control) and KDE's
 * fake-input. It only COUNTS the clipboard bytes (the content is discarded) and sends no key or click.
 * Built by tests/leaks.sh. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "dc.h"
#include "fi.h"
static struct wl_seat *seat; static struct ext_data_control_manager_v1 *dcm; static struct org_kde_kwin_fake_input *fi;
static struct ext_data_control_offer_v1 *sel; static int ntypes; static char mime[128];
static void offer_ev(void *d, struct ext_data_control_offer_v1 *o, const char *m){ ntypes++; if(!mime[0] || !strncmp(m,"text/plain",10)) snprintf(mime,sizeof mime,"%s",m);}
static const struct ext_data_control_offer_v1_listener ol={offer_ev};
static void dev_offer(void*d,struct ext_data_control_device_v1*v,struct ext_data_control_offer_v1*o){ ext_data_control_offer_v1_add_listener(o,&ol,NULL);}
static void dev_sel(void*d,struct ext_data_control_device_v1*v,struct ext_data_control_offer_v1*o){ sel=o;}
static void dev_fin(void*d,struct ext_data_control_device_v1*v){}
static void dev_psel(void*d,struct ext_data_control_device_v1*v,struct ext_data_control_offer_v1*o){}
static const struct ext_data_control_device_v1_listener dl={dev_offer,dev_sel,dev_fin,dev_psel};
static void glob(void*d,struct wl_registry*r,uint32_t n,const char*i,uint32_t v){
 if(!strcmp(i,"wl_seat")) seat=wl_registry_bind(r,n,&wl_seat_interface,1);
 if(!strcmp(i,"ext_data_control_manager_v1")) dcm=wl_registry_bind(r,n,&ext_data_control_manager_v1_interface,1);
 if(!strcmp(i,"org_kde_kwin_fake_input")) fi=wl_registry_bind(r,n,&org_kde_kwin_fake_input_interface,1);}
static void globr(void*d,struct wl_registry*r,uint32_t n){}
static const struct wl_registry_listener rl={glob,globr};
int main(){ struct wl_display*dp=wl_display_connect(NULL); if(!dp){puts("no Wayland");return 1;}
 wl_registry_add_listener(wl_display_get_registry(dp),&rl,NULL); wl_display_roundtrip(dp);
 printf("data-control: %s, fake-input: %s\n", dcm?"bound":"not offered", fi?"bound":"not offered");
 if(fi){ org_kde_kwin_fake_input_authenticate(fi,"test","audit"); if(wl_display_roundtrip(dp)<0) puts("fake-input: compositor REFUSED (protocol error)"); else puts("fake-input: authenticated without error (no input sent)"); }
 if(dcm&&seat){ struct ext_data_control_device_v1*dev=ext_data_control_manager_v1_get_data_device(dcm,seat); ext_data_control_device_v1_add_listener(dev,&dl,NULL);
  if(wl_display_roundtrip(dp)<0){puts("data-control: REFUSED");return 0;} wl_display_roundtrip(dp);
  if(!sel){puts("clipboard empty or not delivered");return 0;}
  int p[2]; pipe(p); ext_data_control_offer_v1_receive(sel,mime,p[1]); wl_display_flush(dp); close(p[1]);
  char b[4096]; long t=0; ssize_t k; while((k=read(p[0],b,sizeof b))>0) t+=k;
  printf("READ THE CLIPBOARD WITHOUT FOCUS: %d types, %ld bytes (content discarded)\n",ntypes,t);}
 return 0;}
