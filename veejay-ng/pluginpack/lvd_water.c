/* 
 * LIBVJE - veejay fx library
 *
 * Copyright(C)2006 Niels Elburg <nelburg@looze.net>
 * See COPYING for software license and distribution details
 */


#define IS_LIVIDO_PLUGIN 
#include 	<livido.h>
LIVIDO_PLUGIN
#include	"utils.h"
#include	"livido-utils.c"
// 40 MB of memory for full pal 4 seconds delay
#define MAXLEN (4*25)
#define PLAY 0
#define REC 1

typedef struct
{
	int	stat;
	signed	char *vtable;
	int *map;
	int *map1;
	int *map2;
	int *map3;
	int map_h;
	int map_w;
	int bgIsSet;	
	int sqrtable[256];
	int period;
	int rain_stat;
	unsigned int drop_prob;
	int drop_prob_increment;
	int drops_per_frame_max;
	int drops_per_frame;
	int drop_power;
	int rate;
} bg_t;

static void setTable(int *sqrtable);

livido_init_f	init_instance( livido_port_t *my_instance )
{
	int w=0;
        int h=0;

	lvd_extract_dimensions( my_instance, "out_channels", &w, &h );
     
	bg_t *b = (bg_t*) livido_malloc( sizeof( bg_t ));
	livido_memset( b,0,sizeof(bg_t));
	
	b->map_h = h/2+1;
	b->map_w = w/2+1;
	b->map = (int*) livido_malloc(sizeof(int) * b->map_h * b->map_w * 3 );
	if(!b->map) return 1;
	b->vtable = (signed char*) livido_malloc(
			sizeof(signed char) * b->map_w * b->map_h * 2 );
	if(!b->vtable) return 1;

	b->map3 = b->map + b->map_w * b->map_h * 2;
	setTable( b->sqrtable );

	livido_memset( b->map, 0, b->map_h * b->map_w * 3 * sizeof(int));
	livido_memset( b->vtable,0,b->map_h * b->map_w * 2 * sizeof(signed char));

	b->map1 = b->map;
	b->map2 = b->map + b->map_w * b->map_h;

	b->stat = 1;
		

	int error = livido_property_set( my_instance, "PLUGIN_private", LIVIDO_ATOM_TYPE_VOIDPTR, 1, &b );
        
        return error;
}



/* from EffecTV:
 * fastrand - fast fake random number generator
 * Warning: The low-order bits of numbers generated by fastrand()
 *          are bad as random numbers. For example, fastrand()%4
 *          generates 1,2,3,0,1,2,3,0...
 *          You should use high-order bits but its not important here
 */
unsigned int wfastrand_val;

unsigned int wfastrand()
{
	return (wfastrand_val=wfastrand_val*1103515245+12345);
}

static void setTable(int *sqrtable)
{
	int i;

	for(i=0; i<128; i++) {
		sqrtable[i] = i*i;
	}
	for(i=1; i<=128; i++) {
		sqrtable[256-i] = -i*i;
	}
}
livido_deinit_f	deinit_instance( livido_port_t *my_instance )
{
	bg_t *b = NULL;
	int error = livido_property_get( my_instance, "PLUGIN_private",
                        0, &b );
#ifdef STRICT_CHECKING
        assert( b != NULL );
#endif
	livido_free( b->map );
	livido_free( b->vtable );
	livido_free( b );
	return LIVIDO_NO_ERROR;
}

static inline void drop(bg_t *b, int power)
{
	int x, y;
	int *p, *q;

	x = wfastrand()%(b->map_w-4)+2;
	y = wfastrand()%(b->map_h-4)+2;
	p = b->map1 + y*b->map_w + x;
	q = b->map2 + y*b->map_w + x;
	*p = power;
	*q = power;
	*(p-b->map_w) = *(p-1) = *(p+1) = *(p+b->map_w) = power/2;
	*(p-b->map_w-1) = *(p-b->map_w+1) = *(p+b->map_w-1) = *(p+b->map_w+1) = power/4;
	*(q-b->map_w) = *(q-1) = *(q+1) = *(q+b->map_w) = power/2;
	*(q-b->map_w-1) = *(q-b->map_w+1) = *(q+b->map_w-1) = *(p+b->map_w+1) = power/4;
}


static void raindrop(bg_t *b)
{
	int i;

	if(b->period == 0) {
		switch(b->rain_stat) {
		case 0:
			b->period = (wfastrand()>>23)+100;
			b->drop_prob = 0;
			b->drop_prob_increment = 0x00ffffff/b->period;
			b->drop_power = (-(wfastrand()>>28)-2)<<16;
			b->drops_per_frame_max = 2<<(wfastrand()>>30); // 2,4,8 or 16
			b->rain_stat = 1;
			break;
		case 1:
			b->drop_prob = 0x00ffffff;
			b->drops_per_frame = 1;
			b->drop_prob_increment = 1;
			b->period = (b->drops_per_frame_max - 1) * 16;
			b->rain_stat = 2;
			break;
		case 2:
			b->period = (wfastrand()>>22)+1000;
			b->drop_prob_increment = 0;
			b->rain_stat = 3;
			break;
		case 3:
			b->period = (b->drops_per_frame_max - 1) * 16;
			b->drop_prob_increment = -1;
			b->rain_stat = 4;
			break;
		case 4:
			b->period = (wfastrand()>>24)+60;
			b->drop_prob_increment = -(b->drop_prob/b->period);
			b->rain_stat = 5;
			break;
		case 5:
		default:
			b->period = (wfastrand()>>23)+500;
			b->drop_prob = 0;
			b->rain_stat = 0;
			break;
		}
	}
	switch(b->rain_stat) {
	default:
	case 0:
		break;
	case 1:
	case 5:
		if((wfastrand()>>8)<b->drop_prob) {
			drop(b, b->drop_power);
		}
		b->drop_prob += b->drop_prob_increment;
		break;
	case 2:
	case 3:
	case 4:
		for(i=b->drops_per_frame/16; i>0; i--) {
			drop(b,b->drop_power);
		}
		b->drops_per_frame += b->drop_prob_increment;
		break;
	}
	b->period--;
}


livido_process_f		process_instance( livido_port_t *my_instance, double timecode )
{
	uint8_t *A[4] = {NULL,NULL,NULL,NULL};
	uint8_t *O[4]= {NULL,NULL,NULL,NULL};
	int palette[3];
	int w[3];
	int h[3];
	int i,j,x,y,dx,dy,v;
	int *p, *q,*r;
	signed char *vp;
	
	int error = lvd_extract_channel_values( my_instance, "in_channels", 0, &w[0], &h[0], A, &palette[0] );
	if( error != LIVIDO_NO_ERROR )
		return LIVIDO_ERROR_HARDWARE; //@ error codes in livido flanky

	error	  = lvd_extract_channel_values( my_instance, "out_channels", 0, &w[2],&h[2], O,&palette[2] );
	if( error != LIVIDO_NO_ERROR )
		return LIVIDO_ERROR_HARDWARE; //@ error codes in livido flanky

#ifdef STRICT_CHECKING
	assert( A[0] != NULL );
	assert( A[1] != NULL );
	assert( A[2] != NULL );
	assert( O[0] != NULL );
	assert( O[1] != NULL );
	assert( O[2] != NULL );
	assert( O[3] != NULL );

#endif
	bg_t *b = NULL;
        error     = livido_property_get( my_instance, "PLUGIN_private", 0, &b );
#ifdef STRICT_CHECKING
	assert( error == NULL );
#endif

        double rate  = lvd_extract_param_number(
                                my_instance,
                                "in_parameters",
                                0 );
        int	loops  = lvd_extract_param_index(
                                my_instance,
                                "in_parameters",
                                1 );
        int	decay  = lvd_extract_param_index(
                                my_instance,
                                "in_parameters",
                                2 );

	int	refresh_rate = 25.0 * rate; //@ pff dont care about fps 

	if( refresh_rate != b->rate  )
	{
		b->rate = refresh_rate;
		livido_memset( b->map, 0, (b->map_h * b->map_w * 2 * sizeof(int)) );
	}
	
	const int len = w[0] * h[0];
	const int wid = w[0];
	const int hei = h[0];

	raindrop(b);

	int wi = b->map_w;
	int hi = b->map_h;
	int hh;

	uint8_t *dest = O[0];
	uint8_t *src  = A[0];
	
	for(i=loops; i>0; i--) {
		/* wave simulation */
		p = b->map1 + wi + 1;
		q = b->map2 + wi + 1;
		r = b->map3 + wi + 1;
		for(y=hi-2; y>0; y--) {
			for(x=wi-2; x>0; x--) {
				hh = *(p-wi-1) + *(p-wi+1) + *(p+wi-1) + *(p+wi+1)
				  + *(p-wi) + *(p-1) + *(p+1) + *(p+wi) - (*p)*9;
				hh = hh >> 3;
				v = *p - *q;
				v += hh - (v >> decay);
				*r = v + *p;
				p++;
				q++;
				r++;
			}
			p += 2;
			q += 2;
			r += 2;
		}

		/* low pass filter */
		p = b->map3 + wi + 1;
		q = b->map2 + wi + 1;
		for(y=hi-2; y>0; y--) {
			for(x=wi-2; x>0; x--) {
				hh = *(p-wi) + *(p-1) + *(p+1) + *(p+wi) + (*p)*60;
				*q = hh >> 6;
				p++;
				q++;
			}
			p+=2;
			q+=2;
		}

		p = b->map1;
		b->map1 = b->map2;
		b->map2 = p;
	}
	
	vp = b->vtable;
	p = b->map1;

	for(y=hi-1; y>0; y--) {
		for(x=wi-1; x>0; x--) {
			/* difference of the height between two voxel. They are twiced to
			 * emphasise the wave. */
			vp[0] = b->sqrtable[((p[0] - p[1])>>(16-1))&0xff]; 
			vp[1] = b->sqrtable[((p[0] - p[wi])>>(16-1))&0xff]; 
			p++;
			vp+=2;
		}
		p++;
		vp+=2;
	}

	hi = hei;
	wi = wid;
	vp = b->vtable;
	for(y=0; y<hi; y+=2) {
		for(x=0; x<wi; x+=2) {
			hh = (int)vp[0];
			v = (int)vp[1];
			dx = x + hh;
			dy = y + v;
			if(dx<0) dx=0;
			if(dy<0) dy=0;
			if(dx>=wi) dx=wi-1;
			if(dy>=hi) dy=hi-1;
			dest[0] = src[dy*wi+dx];

			i = dx;

			dx = x + 1 + (hh+(int)vp[2])/2;
			if(dx<0) dx=0;
			if(dx>=wi) dx=wi-1;
			dest[1] = src[dy*wi+dx];

			dy = y + 1 + (v+(int)vp[b->map_w*2+1])/2;
			if(dy<0) dy=0;
			if(dy>=hi) dy=hh-1;
			dest[wi] = src[dy*wi+i];

			dest[wi+1] = src[dy*wi+dx];
			dest+=2;
			vp+=2;
		}
		dest += wi;
		vp += 2;
	}
	int uv_len = lvd_uv_plane_len( palette[0],w[0],h[0] );

	for( i = 0; i < uv_len ; i ++ )
	{
		O[1][i] = A[1][i];
		O[2][i] = A[2][i];
	}

	
	return LIVIDO_NO_ERROR;
}

livido_port_t	*livido_setup(livido_setup_t list[], int version)

{
	LIVIDO_IMPORT(list);
	livido_port_t *port = NULL;
	livido_port_t *in_params[4];
	livido_port_t *in_chans[2];
	livido_port_t *out_chans[1];
	livido_port_t *info = NULL;
	livido_port_t *filter = NULL;

	info = livido_port_new( LIVIDO_PORT_TYPE_PLUGIN_INFO );
	port = info;

		livido_set_string_value( port, "maintainer", "Niels");
		livido_set_string_value( port, "version","1");
	
	filter = livido_port_new( LIVIDO_PORT_TYPE_FILTER_CLASS );
	livido_set_int_value( filter, "api_version", LIVIDO_API_VERSION );
	livido_set_voidptr_value( filter, "deinit_func", &deinit_instance );
	livido_set_voidptr_value( filter, "init_func", &init_instance );
	livido_set_voidptr_value( filter, "process_func", &process_instance );

	port = filter;

		livido_set_string_value( port, "name", "RaindropTV");	
		livido_set_string_value( port, "description", "RaindropTV");
		livido_set_string_value( port, "author", "FUKUCHI Kentaro,Niels Elburg");
		
		livido_set_int_value( port, "flags", 0);
		livido_set_string_value( port, "license", "GPL2");
		livido_set_int_value( port, "version", 1);
	
	int palettes0[] = {
			LIVIDO_PALETTE_YUV444P,
			LIVIDO_PALETTE_YUV420P,
			LIVIDO_PALETTE_YUV422P,
               		0
	};

	in_params[0] = livido_port_new( LIVIDO_PORT_TYPE_PARAMETER_TEMPLATE );
        port = in_params[0];

                livido_set_string_value(port, "name", "Rate" );
                livido_set_string_value(port, "kind", "NUMBER" );
                livido_set_double_value( port, "min", 0.0 );
                livido_set_double_value( port, "max", 10.0 );
                livido_set_double_value( port, "default", 4.0 );
                livido_set_string_value( port, "description" ,"Refresh rate");
		
	in_params[1] = livido_port_new( LIVIDO_PORT_TYPE_PARAMETER_TEMPLATE );
        port = in_params[1];

                livido_set_string_value(port, "name", "Wave speed" );
                livido_set_string_value(port, "kind", "INDEX" );
		livido_set_int_value( port, "min", 0.0 );
                livido_set_int_value( port, "max", 16.0 );
                livido_set_int_value( port, "default", 2 );
		livido_set_string_value( port, "description" ,"Wave speed");
		
	in_params[2] = livido_port_new( LIVIDO_PORT_TYPE_PARAMETER_TEMPLATE );
        port = in_params[2];

                livido_set_string_value(port, "name", "Decay" );
                livido_set_string_value(port, "kind", "INDEX" );
		livido_set_int_value( port, "min", 0.0 );
                livido_set_int_value( port, "max", 16.0 );
                livido_set_int_value( port, "default", 2 );
		livido_set_string_value( port, "description" ,"Decay");
		
        in_chans[0] = livido_port_new( LIVIDO_PORT_TYPE_CHANNEL_TEMPLATE );
	port = in_chans[0];
            
                livido_set_string_value( port, "name", "Channel A");
           	livido_set_int_array( port, "palette_list", 3, palettes0);
		livido_set_int_value( port, "flags", 0);

	out_chans[0] = livido_port_new( LIVIDO_PORT_TYPE_CHANNEL_TEMPLATE );
	port = out_chans[0];
	
	        livido_set_string_value( port, "name", "Output Channel");
		livido_set_int_array( port, "palette_list", 3, palettes0);
		livido_set_int_value( port, "flags", 0);
	
	livido_set_portptr_array( filter, "in_channel_templates", 1 , in_chans );
	livido_set_portptr_array( filter, "in_parameter_templates",3, in_params );
	livido_set_portptr_array( filter, "out_parameter_templates",0, NULL );
	livido_set_portptr_array( filter, "out_channel_templates", 1, out_chans );

	livido_set_portptr_value(info, "filters", filter);
	return info;
}
