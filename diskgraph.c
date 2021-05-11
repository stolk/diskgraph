// diskgraph.c
//
// by Abraham Stolk.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/ioctl.h>


static int termw=0, termh=0;
static int doubleres=0;
static int blend=1;
static unsigned char termbg[3] = { 0,0,0 };

static int imw=0;
static int imh=0;
static uint32_t* im=0;
static char* legend=0;

static char postscript[256];

static int resized=1;


#if defined(_WIN64)
#	include <windows.h>
static void get_terminal_size(void)
{
	const HANDLE hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo( hStdout, &info );
	termw = info.dwSize.X;
	termh = info.dwSize.Y;
	if ( !termw ) termw = 80;
}
static int oldcodepage=0;
static void set_console_mode(void)
{
	DWORD mode=0;
	const HANDLE hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
	GetConsoleMode( hStdout, &mode );
	mode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode( hStdout, mode );
	oldcodepage = GetConsoleCP();
	SetConsoleCP( 437 );
	doubleres = 1;
}
#else
static void get_terminal_size(void)
{
	FILE* f = popen( "stty size", "r" );
	if ( !f )
	{
		fprintf( stderr, "Failed to determine terminal size using stty.\n" );
		exit( 1 );
	}
	const int num = fscanf( f, "%d %d", &termh, &termw );
	assert( num == 2 );
	pclose( f );
}
static void set_console_mode()
{
	doubleres=1;
}
#endif


typedef uint32_t measurement_t[3];

#define MAXHIST		320
measurement_t hist[ MAXHIST ];
uint32_t head=0;
uint32_t tail=0;
uint32_t maxbw=8192;
uint32_t maxif=16;


static void setup_image(void)
{
	if (im) free(im);
	if (legend) free(legend);

	imw = termw;
	imh = 2*(termh-1);
	const size_t sz = imw*imh*4;
	im = (uint32_t*) malloc(sz);
	memset( im, 0x00, sz );

	legend = (char*) malloc( imw*(imh/2) );
	memset( legend, 0x00, imw*(imh/2) );

	// Draw border into image.
	for ( int y=0; y<imh; ++y )
		for ( int x=0; x<imw; ++x )
		{
			uint32_t b = 0x80 + (y/2) * 0xff / imh;
			uint32_t g = 0xff - b;
			uint32_t r = 0x00;
			uint32_t a = 0xff;
			uint32_t colour = a<<24 | b<<16 | g<<8 | r<<0;
			im[y*imw+x] = x==0 || x==imw-1 || y==0 || y==imh-1 ? colour : 0x0;
		}
}


static void setup_legend(void)
{
#if 0
	// Set up the legend.
	char label_bas[16];
	char label_min[16];
	char label_max[16];
	snprintf( label_bas, sizeof(label_bas), "%3.1f", 10 / 1000000.0f );
	snprintf( label_min, sizeof(label_min), "%3.1f", 1000 / 1000000.0f );
	snprintf( label_max, sizeof(label_max), "%3.1f", 100000 / 1000000.0f );
	int y=1; int x=1;
	sprintf( legend + y * imw + x, "%s", label_max );
	if ( freq_bas[0] != freq_max[0] )
	{
		y=(imh/2) - (barh/2) * freq_bas[0] / (float) freq_max[0];
		sprintf( legend + y * imw + x, "%s", label_bas );
	}
	y=(imh/2) - (barh/2) * freq_min[0] / (float) freq_max[0];
	sprintf( legend + y * imw + x, "%s", label_min );
#endif
}


static void sigwinchHandler(int sig)
{
	resized = 1;
}


static struct termios orig_termios;

void disableRawMode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


void enableRawMode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO);				// Don't echo key presses.
	raw.c_lflag &= ~(ICANON);			// Read by char, not by line.
	raw.c_cc[VMIN] = 0;				// No minimum nr of chars.
	raw.c_cc[VTIME] = 0;				// No waiting time.
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


#define RESETALL  	"\x1b[0m"

#define CURSORHOME	"\x1b[1;1H"

#define CLEARSCREEN	"\x1b[2J"

#define SETFG		"\x1b[38;2;"

#define SETBG		"\x1b[48;2;"


#if defined(_WIN64)
#	define HALFBLOCK "\xdf"		// Uses IBM PC Codepage 437 char 223
#else
#	define HALFBLOCK "▀"		// Uses Unicode char U+2580
#endif

// Note: image has alpha pre-multied. Mimic GL_ONE + GL_ONE_MINUS_SRC_ALPHA
#define BLEND \
{ \
	const int t0 = 255; \
	const int t1 = 255-a; \
	r = ( r * t0 + termbg[0] * t1 ) / 255; \
	g = ( g * t0 + termbg[1] * t1 ) / 255; \
	b = ( b * t0 + termbg[2] * t1 ) / 255; \
}

static void print_image_double_res( int w, int h, unsigned char* data, char* legend )
{
	if ( h & 1 )
		h--;
	const int linesz = 32768;
	char line[ linesz ];

	for ( int y=0; y<h; y+=2 )
	{
		const unsigned char* row0 = data + (y+0) * w * 4;
		const unsigned char* row1 = data + (y+1) * w * 4;
		line[0] = 0;
		for ( int x=0; x<w; ++x )
		{
			char legendchar = legend ? *legend++ : 0;
			// foreground colour.
			strncat( line, "\x1b[38;2;", sizeof(line) - strlen(line) - 1 );
			char tripl[80];
			unsigned char r = *row0++;
			unsigned char g = *row0++;
			unsigned char b = *row0++;
			unsigned char a = *row0++;
			if ( legendchar ) r=g=b=a=0xff;
			if ( blend )
				BLEND
			snprintf( tripl, sizeof(tripl), "%d;%d;%dm", r,g,b );
			strncat( line, tripl, sizeof(line) - strlen(line) - 1 );
			// background colour.
			strncat( line, "\x1b[48;2;", sizeof(line) - strlen(line) - 1 );
			r = *row1++;
			g = *row1++;
			b = *row1++;
			a = *row1++;
			if ( legendchar ) r=g=b=a=0x00;
			if ( blend )
				BLEND
			if ( legendchar )
				snprintf( tripl, sizeof(tripl), "%d;%d;%dm%c", r,g,b,legendchar );
			else
				snprintf( tripl, sizeof(tripl), "%d;%d;%dm" HALFBLOCK, r,g,b );
			strncat( line, tripl, sizeof(line) - strlen(line) - 1 );
		}
		strncat( line, RESETALL, sizeof(line) - strlen(line) - 1 );
		if ( y==h-1 )
			printf( "%s", line );
		else
			puts( line );
	}
}



uint32_t cur_rd;
uint32_t cur_wr;
uint32_t cur_if;

uint32_t dif_rd;
uint32_t dif_wr;

void get_stats( char* fname )
{
	static int firstrun=1;

	FILE* f = fopen( fname, "rb" );
	assert( f );
	char info[16384];
	const int numr = fread( info, 1, sizeof(info), f );
	assert( numr > 0 );
	fclose(f);
	f=0;

	uint32_t v[15];
	int numv = sscanf
	(	
		info,
		"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
		v+0,v+1,v+2,v+3,v+4,v+5,v+6,v+7,v+8,v+9,v+10,v+11,v+12,v+13,v+14
	);
	assert( numv == 15 );

	uint32_t rd = v[2];
	uint32_t wr = v[6];
	if ( firstrun )
	{
		cur_rd = rd;
		cur_wr = wr;
		firstrun=0;
	}
	dif_rd = rd - cur_rd;
	dif_wr = wr - cur_wr;
	cur_rd = rd;
	cur_wr = wr;
	cur_if = v[8];

	hist[tail][0] = dif_rd;
	hist[tail][1] = dif_wr;
	hist[tail][2] = cur_if;
	tail = (tail+1) % MAXHIST;
	if ( tail == head )
		head = (head+1) % MAXHIST;
}


int histsz(void)
{
	int sz = tail - head;
	sz = sz < 0 ? sz + MAXHIST : sz;
	return sz;
}


static void set_postscript(const char* devname)
{
	char fname[128];
	snprintf( fname, sizeof(fname), "/sys/class/block/%s/device/model", devname );
	FILE* f = fopen( fname, "rb" );
	assert( f );
	char nm[128];
	memset(nm, 1, sizeof(nm));
	int l = fread( nm, 1, sizeof(nm), f );
	if ( l>0 && l<sizeof(nm) && nm[l-1] < 32 )
		nm[l-1] = 0;
	fclose(f);
	
	snprintf
	(
		postscript,
		sizeof(postscript),

		SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s"
		SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s"
		SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s"
		SETFG "255;255;255m%s",

 		0x00,0xc0,0x00, 0,0,0, "RD ",
		0xc0,0x00,0x00, 0,0,0, "WR ",
		0xb0,0x60,0x00, 0,0,0, "INFLIGHT ",
		nm
	);
}


int main( int argc, char* argv[] )
{
	if ( argc!=2 )
	{
		fprintf( stderr, "Usage: %s devicename\n", argv[0] );
		exit(1);
	}
	const char* devname = argv[1];
	char fname[128];
	snprintf( fname, sizeof(fname), "/sys/block/%s/stat", devname );
	FILE* f = fopen(fname, "rb");
	if ( !f )
	{
		fprintf( stderr, "Failed to open %s\n", fname );
		exit(2);
	}
	fclose(f);

	get_stats( fname );

	set_postscript( devname );

	// Parse environment variable for terminal background colour.
	const char* imcatbg = getenv( "IMCATBG" );
	if ( imcatbg )
	{
		const int bg = strtol( imcatbg+1, 0, 16 );
		termbg[ 2 ] = ( bg >>  0 ) & 0xff;
		termbg[ 1 ] = ( bg >>  8 ) & 0xff;
		termbg[ 0 ] = ( bg >> 16 ) & 0xff;
		blend = 1;
	}

	// Step 0: Windows cmd.exe needs to be put in proper console mode.
	set_console_mode();


	enableRawMode();

	// Listen to changes in terminal size
	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = 0;
	sa.sa_handler = sigwinchHandler;
	if ( sigaction( SIGWINCH, &sa, 0 ) == -1 )
		perror( "sigaction" );

	int done=0;
	while ( !done )
	{
		if ( resized )
		{
			printf(CLEARSCREEN);
			get_terminal_size();
			setup_image();
			setup_legend();
			resized = 0;
		}

		char c;
		const int numr = read( STDIN_FILENO, &c, 1 );
		if ( numr == 1 && c == 27 )
			done=1;

		int hsz = histsz();
		int overflow_bw=0;
		int overflow_if=0;

		uint32_t quarter_bw = maxbw * 5 * 512 / ( 4 * 1024 * 1024 );
		uint32_t quarter_if = maxif / 4;

		snprintf( legend + imw*(      1) + 1, 80, "%d MeB/s", 4*quarter_bw );
		snprintf( legend + imw*(imh/8*1) + 1, 80, "%d MeB/s", 3*quarter_bw );
		snprintf( legend + imw*(imh/8*2) + 1, 80, "%d MeB/s", 2*quarter_bw );
		snprintf( legend + imw*(imh/8*3) + 1, 80, "%d MeB/s", 1*quarter_bw );

		char lab0[16];
		char lab1[16];
		char lab2[16];
		char lab3[16];
		snprintf( lab0, sizeof(lab0), "%d ops", quarter_if*4 );
		snprintf( lab1, sizeof(lab1), "%d ops", quarter_if*3 );
		snprintf( lab2, sizeof(lab2), "%d ops", quarter_if*2 );
		snprintf( lab3, sizeof(lab3), "%d ops", quarter_if*1 );

		snprintf( legend + imw*(      1) + (imw-1-strlen(lab0)), 80, "%s", lab0 );
		snprintf( legend + imw*(imh/8*1) + (imw-1-strlen(lab1)), 80, "%s", lab1 );
		snprintf( legend + imw*(imh/8*2) + (imw-1-strlen(lab2)), 80, "%s", lab2 );
		snprintf( legend + imw*(imh/8*3) + (imw-1-strlen(lab3)), 80, "%s", lab3 );

		for ( uint32_t i=1; i<imh-1; ++i )
		{
			for ( uint32_t j=0; j<imw-2; ++j )
			{
				if ( j<hsz )
				{
					uint8_t bri = 0x40 + ( 0xb0 * (imw-j) / imw );
					uint8_t a=0xff;
					const uint32_t c_g = (a<<24) | (0x00<<16) | ( bri<<8) | (0x00<<0);
					const uint32_t c_r = (a<<24) | (0x00<<16) | (0x00<<8) | ( bri<<0);
					const uint32_t c_o = 0xff0060b0;
					int h = tail-1-j;
					h = h < 0 ? h+MAXHIST : h;
					uint32_t rd = hist[h][0];
					uint32_t wr = hist[h][1];
					int op = hist[h][2];
					uint32_t x = imw-2-j;
					uint32_t y = imh-1-i;
					uint32_t rd_l = rd * imh / maxbw;
					uint32_t wr_l = wr * imh / maxbw;
					uint32_t op_l = op * imh / maxif;
					if ( rd > maxbw || wr >= maxbw )
						overflow_bw = 1;
					if ( op > maxif )
						overflow_if = 1;
					uint32_t c = (a<<24);
					if ( i == rd_l ) c=c_g;
					if ( i == wr_l ) c=c_r;
					if ( i == op_l ) c=c_o;
					im[ y*imw + x ] = c;
				}
			}
		}
		if ( overflow_bw ) maxbw *= 2;
		if ( overflow_if ) maxif *= 2;

		get_stats(fname);

		printf( CURSORHOME );
		print_image_double_res( imw, imh, (unsigned char*) im, legend );

#if 0
		printf( SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s", 0x00,0xc0,0x00, 0,0,0, "RD " );
		printf( SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s", 0xc0,0x00,0x00, 0,0,0, "WR " );
		printf( SETFG "%d;%d;%dm" SETBG "%d;%d;%dm%s", 0xb0,0x60,0x00, 0,0,0, "INFLIGHT" );
#else
		printf( "%s", postscript );
#endif
		fflush( stdout );

		const int NS_PER_MS = 1000000;
		struct timespec ts  = { 0, 200 * NS_PER_MS };
		nanosleep( &ts, 0 );
	}

	free(im);

#if defined(_WIN64)
	SetConsoleCP( oldcodepage );
#endif
	printf( CLEARSCREEN );

	return 0;
}

