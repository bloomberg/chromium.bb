/* $Id$ */
#ifndef _GPSD_GPS_H_
#define _GPSD_GPS_H_

/* gps.h -- interface of the libgps library */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <inttypes.h>	/* stdint.h would be smaller but not all have it */ 
#include <limits.h>
#include <time.h>
#include <signal.h>
#ifndef S_SPLINT_S
#include <pthread.h>	/* pacifies OpenBSD's compiler */
#endif

#define MAXTAGLEN	8	/* maximum length of sentence tag name */
#define MAXCHANNELS	20	/* maximum GPS channels (*not* satellites!) */
#define SIRF_CHANNELS	12	/* max channels allowed in SiRF format */
#define GPS_PRNMAX	32	/* above this number are SBAS satellites */

/* 
 * The structure describing an uncertainty volume in kinematic space.
 * This is what GPSes are meant to produce; all the other info is 
 * technical impedimenta.
 *
 * All double values use NAN to indicate data not available.
 *
 * Usually all the information in this structure was considered valid
 * by the GPS at the time of update.  This will be so if you are using
 * a GPS chipset that speaks SiRF binary, Garmin binary, or Zodiac binary.
 * This covers over 80% of GPS products in early 2005.
 *
 * If you are using a chipset that speaks NMEA, this structure is updated
 * in bits by GPRMC (lat/lon, track, speed), GPGGA (alt, climb), GPGLL 
 * (lat/lon), and GPGSA (eph, epv).  Most NMEA GPSes take a single fix
 * at the beginning of a 1-second cycle and report the same timestamp in
 * GPRMC, GPGGA, and GPGLL; for these, all info is guaranteed correctly
 * synced to the time member, but you'll get different stages of the same 
 * update depending on where in the cycle you poll.  A very few GPSes, 
 * like the Garmin 48, take a new fix before more than one of of 
 * GPRMC/GPGGA/GPGLL during a single cycle; thus, they may have different 
 * timestamps and some data in this structure can be up to 1 cycle (usually
 * 1 second) older than the fix time.
 *
 * Error estimates are at 95% confidence.
 */
struct gps_fix_t {
    double time;	/* Time of update, seconds since Unix epoch */
    int    mode;	/* Mode of fix */
#define MODE_NOT_SEEN	0	/* mode update not seen yet */
#define MODE_NO_FIX	1	/* none */
#define MODE_2D  	2	/* good for latitude/longitude */
#define MODE_3D  	3	/* good for altitude/climb too */
    double ept;		/* Expected time uncertainty */
    double latitude;	/* Latitude in degrees (valid if mode >= 2) */
    double longitude;	/* Longitude in degrees (valid if mode >= 2) */
    double eph;  	/* Horizontal position uncertainty, meters */
    double altitude;	/* Altitude in meters (valid if mode == 3) */
    double epv;  	/* Vertical position uncertainty, meters */
    double track;	/* Course made good (relative to true north) */
    double epd;		/* Track uncertainty, degrees */
    double speed;	/* Speed over ground, meters/sec */
    double eps;		/* Speed uncertainty, meters/sec */
    double climb;       /* Vertical speed, meters/sec */
    double epc;		/* Vertical speed uncertainty */
};

/*  
 * From the RCTM104 2.x standard:
 *
 * "The 30 bit words (as opposed to 32 bit words) coupled with a 50 Hz
 * transmission rate provides a convenient timing capability where the
 * times of word boundaries are a rational multiple of 0.6 seconds."
 *
 * "Each frame is N+2 words long, where N is the number of message data
 * words. For example, a filler message (type 6 or 34) with no message
 * data will have N=0, and will consist only of two header words. The
 * maximum number of data words allowed by the format is 31, so that
 * the longest possible message will have a total of 33 words."
 */
#define RTCM2_WORDS_MAX	33
#define MAXCORRECTIONS	18	/* max correction count in type 1 or 9 */
#define MAXSTATIONS	10	/* maximum stations in almanac, type 5 */
/* RTCM104 doesn't specify this, so give it the largest reasonable value */
#define MAXHEALTH	(RTCM2_WORDS_MAX-2)	

#ifndef S_SPLINT_S 
/*
 * A nominally 30-bit word (24 bits of data, 6 bits of parity)
 * used both in the GPS downlink protocol described in IS-GPS-200
 * and in the format for DGPS corrections used in RTCM-104v2.
 */
typedef /*@unsignedintegraltype@*/ uint32_t isgps30bits_t;
#endif /* S_SPLINT_S */

    typedef enum {gps, glonass, galileo, unknown} navsystem;

struct rtcm2_t {
    /* header contents */
    unsigned type;	/* RTCM message type */
    unsigned length;	/* length (words) */
    double   zcount;	/* time within hour: GPS time, no leap secs */
    unsigned refstaid;	/* reference station ID */
    unsigned seqnum;	/* message sequence number (modulo 8) */
    unsigned stathlth;	/* station health */

    /* message data in decoded form */
    union {
	struct {
	    unsigned int nentries;
	    struct rangesat_t {		/* data from messages 1 & 9 */
		unsigned ident;		/* satellite ID */
		unsigned udre;		/* user diff. range error */
		unsigned issuedata;	/* issue of data */
		double rangerr;		/* range error */
		double rangerate;	/* range error rate */
	    } sat[MAXCORRECTIONS];
	} ranges;
	struct {		/* data for type 3 messages */
	    bool valid;		/* is message well-formed? */
	    double x, y, z;
	} ecef;
	struct {		/* data from type 4 messages */
	    bool valid;		/* is message well-formed? */
	    navsystem system;
	    enum {local, global, invalid} sense;
	    char datum[6];
	    double dx, dy, dz;
	} reference;
	struct {		/* data from type 5 messages */
	    unsigned int nentries;
	    struct consat_t {
		unsigned ident;		/* satellite ID */
		bool iodl;		/* issue of data */
		unsigned int health;	/* is satellite healthy? */
#define HEALTH_NORMAL		(0)	/* Radiobeacon operation normal */
#define HEALTH_UNMONITORED	(1)	/* No integrity monitor operating */
#define HEALTH_NOINFO		(2)	/* No information available */
#define HEALTH_DONOTUSE		(3)	/* Do not use this radiobeacon */
	       int snr;			/* signal-to-noise ratio, dB */
#define SNR_BAD	-1			/* not reported */
		unsigned int health_en;	/* health enabled */
		bool new_data;		/* new data? */
		bool los_warning;	/* line-of-sight warning */
		unsigned int tou;	/* time to unhealth, seconds */
	    } sat[MAXHEALTH];
	} conhealth;
	struct {		/* data from type 7 messages */
	    unsigned int nentries;
	    struct station_t {
		double latitude, longitude;	/* location */
		unsigned int range;		/* range in km */
		double frequency;		/* broadcast freq */
		unsigned int health;		/* station health */
		unsigned int station_id;	/* of the transmitter */
		unsigned int bitrate;		/* of station transmissions */
	    } station[MAXSTATIONS];
	} almanac;
	/* data from type 16 messages */
	char message[(RTCM2_WORDS_MAX-2) * sizeof(isgps30bits_t)];
	/* data from messages of unknown type */
	isgps30bits_t	words[RTCM2_WORDS_MAX-2];
    } msg_data;
};

/* RTCM3 report structures begin here */

#define RTCM3_MAX_SATELLITES	64
#define RTCM3_MAX_DESCRIPTOR	31
#define RTCM3_MAX_ANNOUNCEMENTS	32

struct rtcm3_rtk_hdr {		/* header data from 1001, 1002, 1003, 1004 */
    /* Used for both GPS and GLONASS, but their timebases differ */
    unsigned int msgnum;	/* Message number */
    unsigned int station_id;	/* Reference Station ID */
    time_t tow;			/* GPS Epoch Time (TOW) in ms, 
				   or GLONASS Epoch Time in ms */
    bool sync;			/* Synchronous GNSS Message Flag */
    ushort satcount;		/* # Satellite Signals Processed */
    bool smoothing;		/* Divergence-free Smoothing Indicator */
    ushort interval;		/* Smoothing Interval */
};

struct rtcm3_basic_rtk {
    unsigned char indicator;	/* Indicator */
    unsigned char channel;	/* Satellite Frequency Channel Number 
				   (GLONASS only) */
    double pseudorange;		/* Pseudorange */
    double rangediff;		/* PhaseRange – Pseudorange in meters */
    unsigned char locktime;	/* Lock time Indicator */
};

struct rtcm3_extended_rtk {
    unsigned char indicator;		/* Indicator */
    unsigned char channel;	/* Satellite Frequency Channel Number 
				   (GLONASS only) */
    double pseudorange;		/* Pseudorange */
    double rangediff;		/* PhaseRange – L1 Pseudorange */
    unsigned char locktime;	/* Lock time Indicator */
    unsigned char ambiguity;	/* Integer Pseudorange 
					   Modulus Ambiguity */
    double CNR;		/* Carrier-to-Noise Ratio */
};

struct rtcm3_network_rtk_header {
    unsigned int msgnum;	/* Message number */
    unsigned int network_id;	/* Network ID */
    unsigned int subnetwork_id;	/* Subnetwork ID */
    time_t time;		/* GPS Epoch Time (TOW) in ms */
    bool multimesg;		/* GPS Multiple Message Indicator */
    unsigned master_id;		/* Master Reference Station ID */
    unsigned aux_id;		/* Auxilary Reference Station ID */
    unsigned char satcount;	/* count of GPS satellites */
};

struct rtcm3_correction_diff {
    unsigned char ident;	/* satellite ID */
    enum {reserved, correct, widelane, uncertain} ambiguity;
    unsigned char nonsync;
    double geometric_diff;	/* Geometric Carrier Phase 
				   Correction Difference (1016, 1017) */
    unsigned char iode;		/* GPS IODE (1016, 1017) */
    double ionospheric_diff;	/* Ionospheric Carrier Phase 
				   Correction Difference (1015, 1017) */
};

struct rtcm3_t {
    /* header contents */
    unsigned type;	/* RTCM 3.x message type */
    unsigned length;	/* payload length, inclusive of checksum */

    union {
	/* 1001-1013 were present in the 3.0 version */
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_basic_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1001;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_extended_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1002;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_basic_rtk L1;
		struct rtcm3_basic_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1003;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1004;
	struct {
	    unsigned int station_id;	/* Reference Station ID */
	    navsystem system;		/* Which system is it? */
	    bool reference_station;	/* Reference-station indicator */
	    bool single_receiver;	/* Single Receiver Oscillator */
	    double ecef_x, ecef_y, ecef_z;	/* ECEF antenna location */
	} rtcm3_1005;
	struct {
	    unsigned int station_id;	/* Reference Station ID */
	    navsystem system;		/* Which system is it? */
	    bool reference_station;	/* Reference-station indicator */
	    bool single_receiver;	/* Single Receiver Oscillator */
	    double ecef_x, ecef_y, ecef_z;	/* ECEF antenna location */
	    double height;			/* Antenna height */
	} rtcm3_1006;
	struct {
	    unsigned int station_id;	/* Reference Station ID */
	    char descriptor[RTCM3_MAX_DESCRIPTOR+1];	/* Description string */
	    unsigned char setup_id;
	} rtcm3_1007;
	struct {
	    unsigned int station_id;	/* Reference Station ID */
	    char descriptor[RTCM3_MAX_DESCRIPTOR+1];	/* Description string */
	    unsigned char setup_id;
	    char serial[RTCM3_MAX_DESCRIPTOR+1];	/* Serial # string */
	} rtcm3_1008;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_basic_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1009;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_extended_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1010;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1011;
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1012;
	struct {
	    unsigned int msgnum;	/* Message number */
	    unsigned int station_id;	/* Reference Station ID */
	    unsigned short mjd;		/* Modified Julian Day (MJD) Number */
	    unsigned int sod;		/* Seconds of Day (UTC) */
	    unsigned char leapsecs;	/* Leap Seconds, GPS-UTC */
	    unsigned char ncount;	/* Count of announcements to follow */
	    struct {
		unsigned short id;
		bool sync;
		unsigned short interval;
	    } announcements[RTCM3_MAX_ANNOUNCEMENTS];
	} rtcm3_1013;
	/* 1014-1017 were added in the 3.1 version */
	struct {
	    unsigned int msgnum;	/* Message number */
	    unsigned int network_id;	/* Network ID */
	    unsigned int subnetwork_id;	/* Subnetwork ID */
	    unsigned char stationcount;	/* # auxiliary stations transmitted */
	    unsigned int master_id;	/* Master Reference Station ID */
	    unsigned int aux_id;	/* Auxilary Reference Station ID */
	    double d_lat, d_lon, d_alt;	/* Aux-master location delta */
	} rtcm3_1014;
	struct {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1015;
	struct {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1016;
	struct {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1017;
	struct {
	    unsigned int msgnum;	/* Message number */
	    unsigned int ident;		/* Satellite ID */
	    unsigned int week;		/* GPS Week Number */
	    unsigned char sv_accuracy;	/* GPS SV ACCURACY */
	    enum {reserved_code, p, ca, l2c} code;
	    double idot;
	    unsigned char iode;
	    /* ephemeris fields, not scaled */
	    unsigned int t_sub_oc;
	    signed int a_sub_f2;
	    signed int a_sub_f1;
	    signed int a_sub_f0;
	    unsigned int iodc;
	    signed int C_sub_rs;
	    signed int delta_sub_n;
	    signed int M_sub_0;
	    signed int C_sub_uc;
	    unsigned int e;
	    signed int C_sub_us;
	    unsigned int sqrt_sub_A;
	    unsigned int t_sub_oe;
	    signed int C_sub_ic;
	    signed int OMEGA_sub_0;
	    signed int C_sub_is;
	    signed int i_sub_0;
	    signed int C_sub_rc;
	    signed int argument_of_perigee;
	    signed int omegadot;
	    signed int t_sub_GD;
	    unsigned char sv_health;
	    bool p_data;
	    bool fit_interval;
	} rtcm3_1019;
	struct {
	    unsigned int msgnum;	/* Message number */
	    unsigned int ident;		/* Satellite ID */
	    unsigned short channel;	/* Satellite Frequency Channel Number */
	    /* ephemeris fields, not scaled */
	    bool C_sub_n;
	    bool health_avAilability_indicator;
	    unsigned char P1;
	    unsigned short t_sub_k;
	    bool msb_of_B_sub_n;
	    bool P2;
	    bool t_sub_b;
	    signed int x_sub_n_t_of_t_sub_b_prime;
	    signed int x_sub_n_t_of_t_sub_b;
	    signed int x_sub_n_t_of_t_sub_b_prime_prime;
	    signed int y_sub_n_t_of_t_sub_b_prime;
	    signed int y_sub_n_t_of_t_sub_b;
	    signed int y_sub_n_t_of_t_sub_b_prime_prime;
	    signed int z_sub_n_t_of_t_sub_b_prime;
	    signed int z_sub_n_t_of_t_sub_b;
	    signed int z_sub_n_t_of_t_sub_b_prime_prime;
	    bool P3;
	    signed int gamma_sub_n_of_t_sub_b;
	    unsigned char MP;
	    bool Ml_n;
	    signed int tau_n_of_t_sub_b;
	    signed int M_delta_tau_sub_n;
	    unsigned int E_sub_n;
	    bool MP4;
	    unsigned char MF_sub_T;
	    unsigned char MN_sub_T;
	    unsigned char MM;
	    bool additioinal_data_availability;
	    unsigned int N_sup_A;
	    unsigned int tau_sub_c;
	    unsigned int M_N_sub_4;
	    signed int M_tau_sub_GPS;
	    bool M_l_sub_n;
	} rtcm3_1020;
	struct {
	    unsigned int msgnum;	/* Message number */
	    unsigned int station_id;	/* Reference Station ID */
	    unsigned short mjd;		/* Modified Julian Day (MJD) Number */
	    unsigned int sod;		/* Seconds of Day (UTC) */
	    unsigned char len;		/* # Chars to follow */
	    unsigned char unicode_units;
	    unsigned char text[128];
	} rtcm3_1029;
    } rtcmtypes;
};

typedef /*@unsignedintegraltype@*/ unsigned int gps_mask_t;

struct gps_data_t {
    gps_mask_t set;	/* has field been set since this was last cleared? */
#define ONLINE_SET	0x00000001u
#define TIME_SET	0x00000002u
#define TIMERR_SET	0x00000004u
#define LATLON_SET	0x00000008u
#define ALTITUDE_SET	0x00000010u
#define SPEED_SET	0x00000020u
#define TRACK_SET	0x00000040u
#define CLIMB_SET	0x00000080u
#define STATUS_SET	0x00000100u
#define MODE_SET	0x00000200u
#define HDOP_SET  	0x00000400u
#define VDOP_SET  	0x00000800u
#define PDOP_SET  	0x00001000u
#define TDOP_SET	0x00002000u
#define GDOP_SET	0x00004000u
#define DOP_SET		(HDOP_SET|VDOP_SET|PDOP_SET|TDOP_SET|GDOP_SET)
#define HERR_SET	0x00008000u
#define VERR_SET	0x00010000u
#define PERR_SET	0x00020000u
#define ERR_SET		(HERR_SET | VERR_SET | PERR_SET)
#define SATELLITE_SET	0x00040000u
#define PSEUDORANGE_SET	0x00080000u
#define USED_SET	0x00100000u
#define SPEEDERR_SET	0x00200000u
#define TRACKERR_SET	0x00400000u
#define CLIMBERR_SET	0x00800000u
#define DEVICE_SET	0x01000000u
#define DEVICELIST_SET	0x02000000u
#define DEVICEID_SET	0x04000000u
#define ERROR_SET	0x08000000u
#define CYCLE_START_SET	0x10000000u
#define RTCM2_SET	0x20000000u
#define RTCM3_SET	0x40000000u
#define FIX_SET		(TIME_SET|MODE_SET|TIMERR_SET|LATLON_SET|HERR_SET|ALTITUDE_SET|VERR_SET|TRACK_SET|TRACKERR_SET|SPEED_SET|SPEEDERR_SET|CLIMB_SET|CLIMBERR_SET)
    double online;		/* NZ if GPS is on line, 0 if not.
				 *
				 * Note: gpsd clears this flag when sentences
				 * fail to show up within the GPS's normal
				 * send cycle time. If the host-to-GPS 
				 * link is lossy enough to drop entire
				 * sentences, this flag will be
				 * prone to false negatives.
				 */

    struct gps_fix_t	fix;		/* accumulated PVT data */

    double separation;		/* Geoidal separation, MSL - WGS84 (Meters) */

    /* GPS status -- always valid */
    int    status;		/* Do we have a fix? */
#define STATUS_NO_FIX	0	/* no */
#define STATUS_FIX	1	/* yes, without DGPS */
#define STATUS_DGPS_FIX	2	/* yes, with DGPS */

    /* precision of fix -- valid if satellites_used > 0 */
    int satellites_used;	/* Number of satellites used in solution */
    int used[MAXCHANNELS];	/* PRNs of satellites used in solution */
    double pdop, hdop, vdop, tdop, gdop;	/* Dilution of precision */

    /* redundant with the estimate elments in the fix structure */
    double epe;  /* spherical position error, 95% confidence (meters)  */

    /* satellite status -- valid when satellites > 0 */
    int satellites;		/* # of satellites in view */
    int PRN[MAXCHANNELS];	/* PRNs of satellite */
    int elevation[MAXCHANNELS];	/* elevation of satellite */
    int azimuth[MAXCHANNELS];	/* azimuth */
    int ss[MAXCHANNELS];	/* signal-to-noise ratio (dB) */

#if 0	/* not yet used or filled in */
    /* measurement data */
    double pseudorange[MAXCHANNELS];	/* meters */
    double deltarange[MAXCHANNELS];	/* meters/sec */
    double doppler[MAXCHANNELS];	/* Hz */
    unsigned satstat[MAXCHANNELS];	/* tracking status */
#define SAT_ACQUIRED	0x01		/* satellite acquired */
#define SAT_CODE_TRACK	0x02		/* code-tracking loop acquired */
#define SAT_CARR_TRACK	0x04		/* carrier-tracking loop acquired */
#define SAT_DATA_SYNC	0x08		/* data-bit synchronization done */
#define SAT_FRAME_SYNC	0x10		/* frame synchronization done */
#define SAT_EPHEMERIS	0x20		/* ephemeris collected */
#define SAT_FIX_USED	0x40		/* used for position fix */
#endif

#if defined(TNT_ENABLE) || defined(OCEANSERVER_ENABLE)
    /* compass status -- TrueNorth (and any similar) devices only */
    char headingStatus;
    char pitchStatus;
    char rollStatus;
    double horzField;   /* Magnitude of horizontal magnetic field */
#endif

#ifdef OCEANSERVER_ENABLE
    double magnetic_length; /* unitvector sqrt(x^2 + y^2 +z^2) */
    double magnetic_field_x;
    double magnetic_field_y;
    double magnetic_field_z;
    double acceleration_length; /* unitvector sqrt(x^2 + y^2 +z^2) */
    double acceleration_field_x;
    double acceleration_field_y;
    double acceleration_field_z;
    double gyro_output_x;
    double gyro_output_y;
    double temperature;
#endif

    /* where and what gpsd thinks the device is */
    char	gps_device[PATH_MAX];	/* only valid if non-null. */
    char	*gps_id;	/* only valid if non-null. */
    unsigned int baudrate, parity, stopbits;	/* RS232 link parameters */
    unsigned int driver_mode;	/* whether driver is in native mode or not */

    /* RTCM-104 data */
    struct rtcm2_t	rtcm2;
    struct rtcm3_t	rtcm3;
    
    /* device list */
    int ndevices;		/* count of available devices */
    char **devicelist;		/* list of pathnames */

    /* profiling data for last sentence */
    bool profiling;		/* profiling enabled? */
    char tag[MAXTAGLEN+1];	/* tag of last sentence processed */
    size_t sentence_length;	/* character count of last sentence */
    double sentence_time;	/* sentence timestamp */
    double d_xmit_time;		/* beginning of sentence transmission */
    double d_recv_time;		/* daemon receipt time (-> E1+T1) */
    double d_decode_time;	/* daemon end-of-decode time (-> D1) */
    double poll_time;		/* daemon poll time (-> W) */
    double emit_time;		/* emission time (-> E2) */
    double c_recv_time;		/* client receipt time (-> T2) */
    double c_decode_time;	/* client end-of-decode time (-> D2) */
    double cycle, mincycle;	/* refresh cycle time in seconds */

    /* these members are private */
    int gps_fd;			/* socket or file descriptor to GPS */
    void (*raw_hook)(struct gps_data_t *, char *, size_t len, int level);/* Raw-mode hook for GPS data. */
    void (*thread_hook)(struct gps_data_t *, char *, size_t len, int level);/* Thread-callback hook for GPS data. */
};

extern /*@null@*/ struct gps_data_t *gps_open(const char *host, const char *port);
int gps_close(struct gps_data_t *);
int gps_query(struct gps_data_t *gpsdata, const char *fmt, ... );
int gps_poll(struct gps_data_t *gpsdata);
void gps_set_raw_hook(struct gps_data_t *gpsdata, void (*hook)(struct gps_data_t *sentence, char *buf, size_t len, int level));
int gps_set_callback(struct gps_data_t *gpsdata, void (*callback)(struct gps_data_t *sentence, char *buf, size_t len, int level), pthread_t *handler);
int gps_del_callback(struct gps_data_t *gpsdata, pthread_t *handler);

enum unit {unspecified, imperial, nautical, metric};
enum unit gpsd_units(void);
enum deg_str_type { deg_dd, deg_ddmm, deg_ddmmss };
extern /*@observer@*/ char *deg_to_str( enum deg_str_type type,  double f);

extern void gps_clear_fix(/*@ out @*/struct gps_fix_t *);
extern void gps_merge_fix(/*@ out @*/struct gps_fix_t *, 
			  gps_mask_t,
			  /*@ in @*/struct gps_fix_t *);
extern unsigned int gps_valid_fields(/*@ in @*/struct gps_fix_t *);
extern char *gps_show_transfer(int);

extern time_t mkgmtime(register struct tm *);
extern double timestamp(void);
extern double iso8601_to_unix(char *);
extern /*@observer@*/char *unix_to_iso8601(double t, /*@ out @*/char[], size_t len);
extern double gpstime_to_unix(int, double);
extern void unix_to_gpstime(double, /*@out@*/int *, /*@out@*/double *);
extern double earth_distance(double, double, double, double);
extern double wgs84_separation(double, double);

/* some multipliers for interpreting GPS output */
#define METERS_TO_FEET	3.2808399	/* Meters to U.S./British feet */
#define METERS_TO_MILES	0.00062137119	/* Meters to miles */
#define KNOTS_TO_MPH	1.1507794	/* Knots to miles per hour */
#define KNOTS_TO_KPH	1.852		/* Knots to kilometers per hour */
#define KNOTS_TO_MPS	0.51444444	/* Knots to meters per second */
#define MPS_TO_KPH	3.6		/* Meters per second to klicks/hr */
#define MPS_TO_MPH	2.2369363	/* Meters/second to miles per hour */
#define MPS_TO_KNOTS	1.9438445	/* Meters per second to knots */
/* miles and knots are both the international standard versions of the units */

/* angle conversion multipliers */
#define GPS_PI      	3.1415926535897932384626433832795029
#define RAD_2_DEG	57.2957795130823208767981548141051703
#define DEG_2_RAD	0.0174532925199432957692369076848861271

/* gps_open() errno return values */
#define NL_NOSERVICE	-1	/* can't get service entry */
#define NL_NOHOST	-2	/* can't get host entry */
#define NL_NOPROTO	-3	/* can't get protocol entry */
#define NL_NOSOCK	-4	/* can't create socket */
#define NL_NOSOCKOPT	-5	/* error SETSOCKOPT SO_REUSEADDR */
#define NL_NOCONNECT	-6	/* can't connect to host */

#define DEFAULT_GPSD_PORT	"2947"	/* IANA assignment */
#define DEFAULT_RTCM_PORT	"2101"	/* IANA assignment */

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

/* gps.h ends here */
#endif /* _GPSD_GPS_H_ */
