#include <hubble/sat/ephemeris.h>

#include <math.h>
#include <string.h>
#include <errno.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define HUBBLE_EARTH_RADIUS              6378136.999954619 /* Earth radius at equator */
#define HUBBLE_EARTH_ROTATION_RATE       7.292115855377074e-05 /* rad / s */
#define HUBBLE_TEME_REF_DATETIME_2027    1798761600
#define HUBBLE_TEME_ANGLE_2027           1.7526971469712507
#define HUBBLE_TWO_PI_DEGREES            360
#define HUBBLE_PI_DEGREES                180
#define HUBBLE_ELEVATION_ANGLE_TOLERANCE 30
#define HUBBLE_SAT_ELEVATION             6892550.590445475

#define HUBBLE_PI_2                      1.57079632679489661923  /* PI / 2 */
#define HUBBLE_PI_4                      0.785398163397448309616 /* PI / 4 */
#define HUBBLE_INV_PI                    0.31830988618379067154  /* 1 / PI */

/* Converts an angle in degrees to radians */
#define _DEG2RAD(_deg)                   ((_deg) * (M_PI / HUBBLE_PI_DEGREES))

/* Converts an angle in radians to degrees */
#define _RAD2DEG(_rad)                   ((_rad) * (HUBBLE_PI_DEGREES / M_PI))

struct crossing_info {
	uint64_t t;
	double lon;
};

static const struct {
	double radius;
	double mu;
	double J2;
	double earth_rotation_rate;
	uint64_t teme_ref_datetime_2027;
	double teme_angle_2027;
} earth = {
	.radius = HUBBLE_EARTH_RADIUS,
	.mu = 398600441800000.0,
	.J2 = 0.00108262668,
	.earth_rotation_rate = HUBBLE_EARTH_ROTATION_RATE,
	.teme_ref_datetime_2027 = HUBBLE_TEME_REF_DATETIME_2027,
	.teme_angle_2027 = HUBBLE_TEME_ANGLE_2027,
};

#ifdef CONFIG_HUBBLE_SAT_NETWORK_SMALL

static double _atan_poly(double u)
{
	double p = 0.1111111111111111;    /* 1/9 */
	double t = u * u;

	p = -0.14285714285714285 + t * p; /* -1/7 */
	p = 0.2 + t * p;                  /* 1/5 */
	p = -0.3333333333333333 + t * p;  /* -1/3 */
	p = 1.0 + t * p;                  /* 1 */

	return u * p;
}

static double _atan_small(double x)
{
	const double TAN22_5 = 0.41421356237309503; /* tan(pi/8) */
	const double TAN67_5 = 2.414213562373095;   /* tan(3*pi/8) */

	double ax = x < 0.0 ? -x : x;
	double y;

	if (ax <= TAN22_5) {
		y = _atan_poly(ax);
	} else if (ax >= TAN67_5) {
		y = HUBBLE_PI_2 - _atan_poly(1.0 / ax);
	} else {
		double u = (ax - 1.0) / (ax + 1.0);
		y = HUBBLE_PI_4 + _atan_poly(u);
	}

	return x < 0.0 ? -y : y;
}

/* Polynomial for sin on [-π/4, π/4], Horner form */
static double _sin_poly(double z, double x)
{
	// z = x^2
	return x *
	       (1.0 +
		z * (-1.66666666666666324348e-1 +
		     z * (8.33333333332248946124e-3 +
			  z * (-1.98412698298579493134e-4 +
			       z * (2.75573137070700676789e-6 +
				    z * (-2.50507602534068634195e-8 +
					 z * (1.58969099521155010221e-10)))))));
}

/* Polynomial for cos on [-π/4, π/4] */
static double _cos_poly(double z)
{
	return 1.0 +
	       z * (-0.5 +
		    z * (4.16666666666666019037e-2 +
			 z * (-1.38888888888741095749e-3 +
			      z * (2.48015872894767294178e-5 +
				   z * (-2.75573143513906633035e-7 +
					z * (2.08757232129817482790e-9 +
					     z * (-1.13596475577881948265e-11)))))));
}

/* Range reduction: reduce x to quadrant and remainder in [-π/4, π/4] */
static void _range_reduce(double x, int *q, double *r)
{
	/* Reduce x/π to integer multiple of 0.5 */
	double n = nearbyint(
		x * (0.5 * HUBBLE_INV_PI)); /* round to nearest multiple of π/2 */

	*q = (int)n;
	*r = x - n * HUBBLE_PI_2;
}

static double _sin_small(double x)
{
	int q;
	double r, z;

	_range_reduce(x, &q, &r);
	z = r * r;

	switch (q & 3) {
	case 0:
		return _sin_poly(z, r);
	case 1:
		return _cos_poly(z);
	case 2:
		return -_sin_poly(z, r);
	default:
		return -_cos_poly(z);
	}
}

static double _cos_small(double x)
{
	int q;
	double r, z;
	_range_reduce(x, &q, &r);

	z = r * r;

	switch (q & 3) {
	case 0:
		return _cos_poly(z);
	case 1:
		return -_sin_poly(z, r);
	case 2:
		return -_cos_poly(z);
	default:
		return _sin_poly(z, r);
	}
}

static double _fmod_small(double x, double y)
{
	double q, qi;

	if (y == 0.0) {
		return NAN; /* domain error, same as standard fmod */
	}

	q = (x / y);
	qi = (q >= 0.0) ? floor(q) : ceil(q);

	return x - qi * y;
}

static double _sqrt_small(double x)
{
	int scaled = 0;
	union {
		double d;
		uint64_t u;
	} v;

	/* Handle 0, negatives, Inf/NaN up front (small & IEEE-friendly) */
	if (x <= 0.0) {
		/* sqrt(0) = 0 */
		if (x == 0.0) {
			return 0.0;
		}
		/* negative → NaN */
		return 0.0 / 0.0;
	}

	if (x == (x + x)) {
		/* Inf → Inf; NaN passes through below */
		return x;
	}

	v.d = x;

	/* Scale subnormals up to normal range: x *= 2^52, later scale
	 * result by 2^-26
	 */

	/* exponent == 0 => subnormal */
	if ((v.u & 0x7ff0000000000000ULL) == 0) {
		/* 2^52 */
		x *= 4503599627370496.0;
		scaled = 1;
		v.d = x;
	}

	/* Quake-style inverse sqrt seed (works well for all normals) */
	v.u = 0x5fe6eb50c7b537a9ULL - (v.u >> 1); // initial 1/sqrt(x)
	double y = v.d;

	/* Two Newton steps for 1/sqrt: y *= (1.5 - 0.5*x*y*y) */
	y = y * (1.5 - (0.5 * x * y * y));
	y = y * (1.5 - (0.5 * x * y * y));

	/* Turn into sqrt and polish once with Heron (Newton on sqrt) */
	double s = x * y;
	s = 0.5 * (s + (x / s));

	/* Undo subnormal scaling: sqrt(x * 2^52) = sqrt(x) * 2^26 ⇒
	 * multiply by 2^-26
	 */
	if (scaled) {
		/* 2^-26 */
		s *= 1.4901161193847656e-8;
	}

	return s;
}

static double _tan_small(double x)
{
	return _sin_small(x) / _cos_small(x);
}

static inline double _asin_small(double x)
{
	double denom;

	/* clamp to [-1,1] to avoid NaNs from rounding */
	if (x > 1.0) {
		x = 1.0;
	}

	if (x < -1.0) {
		x = -1.0;
	}

	/* identity: asin(x) = atan( x / sqrt(1 - x^2) ) */
	denom = _sqrt_small(fmax(0.0, 1.0 - x * x));
	if (denom == 0.0) { // x is +-1
		return copysign(HUBBLE_PI_2, x);
	}

	return _atan_small(x / denom);
}

#define _cos  _cos_small
#define _sin  _sin_small
#define _sqrt _sqrt_small
#define _atan _atan_small
#define _asin _asin_small
#define _tan  _tan_small
#define _fmod _fmod_small

#else

#define _cos  cos
#define _sin  sin
#define _sqrt sqrt
#define _atan atan
#define _asin asin
#define _tan  tan
#define _fmod fmod

#endif /* CONFIG_HUBBLE_SAT_NETWORK_SMALL */

static double _signed_fmod(double x, double y)
{
	double ret;

	if (y == 0.0) {
		return NAN;
	}

	ret = _fmod(x, y);
	if ((ret != 0) && ((y < 0 && ret > 0) || (y > 0 && ret < 0))) {
		ret += y;
	}

	return ret;
}

/* Normalizes an angle to the range [0, 2π) */
static double _zero_to_2pi(double angle)
{
	if (angle < 0.0) {
		return angle + (2.00 * M_PI);
	}

	return _fmod(angle, (2.00 * M_PI));
}

/* Normalizes an angle to the range [-180, 180) */
static double _minus_180_to_180(double angle)
{
	return _signed_fmod(angle + HUBBLE_PI_DEGREES, HUBBLE_TWO_PI_DEGREES) -
	       HUBBLE_PI_DEGREES;
}

static double _zero_to_360(double angle)
{
	return _signed_fmod(angle, HUBBLE_TWO_PI_DEGREES);
}

/* Computes mean anomaly from true anomaly (theta) */
static double _anomaly_from_theta_mean(double e, double theta)
{
	double E, me;

	if (e == 0.0) {
		return theta;
	}

	E = 2 * _atan(_sqrt((1 - e) / (1 + e)) * _tan(theta / 2));
	me = E - e * _sin(E);

	return _zero_to_2pi(me);
}

/* Gets the time of the ascending node for a given orbit count */
static uint64_t _anode_time_get(const struct orbit_info *info, int count)
{
	double dt;

	if (info->ndot == 0.0) {
		dt = count / info->n0;
	} else {
		dt = (_sqrt(info->n0 * info->n0 + 2 * info->ndot * count) -
		      info->n0) /
		     info->ndot;
	}

	return (uint64_t)(info->t0 + lround(dt));
}

/* Gets the orbit count at a given time */
static int _orbit_count_get(const struct orbit_info *info, uint64_t t)
{
	int64_t dt = t - info->t0;

	return (int)((info->n0 * dt) + (0.5 * info->ndot) * dt * dt);
}

/* Gets longitude from right ascension and time */
static double _longitude_get(double ra, uint64_t t)
{
	int64_t dt = t - earth.teme_ref_datetime_2027;
	double lon_rad =
		ra - earth.teme_angle_2027 - earth.earth_rotation_rate * dt;

	return _minus_180_to_180(_RAD2DEG(lon_rad));
}

/* Gets the crossings for a target latitude */
static int _tll_crossings_get(const struct orbit_info *orbit, double tll,
			      int orbit_count, struct crossing_info result[2])
{
	double latrad = _DEG2RAD(tll);
	double inclination = _DEG2RAD(orbit->inclination);
	double lam1, lam2, me0, me1, me2, ra1, ra2, aop, orbit_period, raan;
	uint64_t anode_time;
	int64_t dt_anode;

	if ((inclination < 0) || (inclination > M_PI)) {
		return -1;
	}

	if (fabs(_sin(inclination)) <= fabs(_sin(latrad))) {
		return -1;
	}

	anode_time = _anode_time_get(orbit, orbit_count);
	dt_anode = (int64_t)anode_time - orbit->t0;
	raan = orbit->raan0 + (orbit->raandot * dt_anode);
	aop = orbit->aop0 + (orbit->aopdot * dt_anode);
	orbit_period = 1.0 / (orbit->n0 + (orbit->ndot * dt_anode));
	if (latrad >= 0) {
		ra1 = raan + _asin(_tan(latrad) / _tan(inclination));
		ra2 = raan + M_PI - _asin(_tan(latrad) / _tan(inclination));
	} else {
		ra2 = raan + _asin(_tan(latrad) / _tan(inclination));
		ra1 = raan + M_PI - _asin(_tan(latrad) / _tan(inclination));
	}

	if (latrad >= 0) {
		lam1 = _asin(_sin(latrad) / _sin(inclination));
		lam2 = M_PI - lam1;
	} else {
		lam1 = M_PI - _asin(_sin(latrad) / _sin(inclination));
		lam2 = (3 * M_PI) - lam1;
	}

	if ((lam1 < 0) || (lam1 >= (2 * M_PI))) {
		return -1;
	}

	if ((lam2 < 0) || (lam2 >= (2 * M_PI))) {
		return -1;
	}

	if (lam1 >= lam2) {
		return -1;
	}

	me0 = _anomaly_from_theta_mean(orbit->eccentricity, -aop);
	me1 = _anomaly_from_theta_mean(orbit->eccentricity, lam1 - aop);
	me2 = _anomaly_from_theta_mean(orbit->eccentricity, lam2 - aop);

	result[0].t =
		anode_time +
		(uint64_t)lround(_signed_fmod(
			orbit_period * (me1 - me0) / (2 * M_PI), orbit_period));
	result[0].lon = _longitude_get(ra1, result[0].t);
	result[1].t =
		anode_time +
		(uint64_t)lround(_signed_fmod(
			orbit_period * (me2 - me0) / (2 * M_PI), orbit_period));
	result[1].lon = _longitude_get(ra2, result[1].t);

	return 0;
}

static double _lon_tolerance_get(double lat)
{
	double A, C, b, B;

	A = _DEG2RAD(HUBBLE_ELEVATION_ANGLE_TOLERANCE + 90);

	C = _asin(earth.radius * _sin(A) / HUBBLE_SAT_ELEVATION);
	b = earth.radius * _cos(M_PI - _asin(HUBBLE_SAT_ELEVATION *
					     (_sin(C) / earth.radius))) +
	    (HUBBLE_SAT_ELEVATION * (_cos(C)));
	B = _asin(b * sin(C) / earth.radius);

	return _RAD2DEG(_asin((earth.radius * _sin(B)) /
			      (earth.radius * _cos(_DEG2RAD(lat)))));
}

/*
 * Helper function to calculate the next pass for ascending or
 * descending crossings
 */
static int _next_pass_get(const struct orbit_info *orbit, bool ascending,
			  double delta_lon, double lon_tol,
			  const struct ground_info *ground,
			  struct crossing_info crossings[2],
			  struct hubble_pass_info *pass, uint64_t t)

{
	int orbit_count, index;
	double dt = _DEG2RAD(delta_lon) / earth.earth_rotation_rate;

	/* Determine the index for ascending or descending crossing */
	index = ascending ? 0 : 1;
	orbit_count = _orbit_count_get(
		orbit, crossings[index].t + (uint64_t)lround(dt));

	/* Get the crossings for the updated orbit count */
	if (_tll_crossings_get(orbit, ground->lat, orbit_count, crossings) != 0) {
		return -1;
	}

	/* Iterate until a valid pass is found */
	while (pass->t == 0 &&
	       (HUBBLE_TWO_PI_DEGREES - _zero_to_360(ground->lon - lon_tol -
						     crossings[index].lon) <
		HUBBLE_PI_DEGREES)) {
		if ((fabs(_minus_180_to_180(crossings[index].lon - ground->lon)) <=
		     lon_tol) &&
		    (crossings[index].t > t)) {
			pass->t = crossings[index].t;
			pass->lon = crossings[index].lon;
			pass->ascending = (ascending) ? ground->lat > 0
						      : ground->lat <= 0;
		} else {
			orbit_count++;
			if (_tll_crossings_get(orbit, ground->lat, orbit_count,
					       crossings) != 0) {
				return -1;
			}
		}
	}

	return 0;
}

int hubble_next_pass_get(const struct orbit_info *orbit, uint64_t t,
			 const struct ground_info *ground,
			 struct hubble_pass_info *pass)
{
	double lon_tol;
	struct crossing_info crossings[2];
	int orbit_count;

	/* Basic sanity check */
	if ((orbit == NULL) || (ground == NULL) || (pass == NULL)) {
		return -EINVAL;
	}

	lon_tol = _lon_tolerance_get(ground->lat);

	orbit_count = _orbit_count_get(orbit, t);
	if (orbit_count <= 0) {
		return -1;
	}

	if (_tll_crossings_get(orbit, ground->lat, orbit_count, crossings) != 0) {
		return -1;
	}

	while (crossings[0].t <= t) {
		orbit_count++;
		if (_tll_crossings_get(orbit, ground->lat, orbit_count,
				       crossings) != 0) {
			return -1;
		}
	}

	memset(pass, 0, sizeof(*pass));

	if ((fabs(_minus_180_to_180(crossings[0].lon - ground->lon)) <= lon_tol) &&
	    (crossings[0].t > t)) {
		pass->t = crossings[0].t;
		pass->lon = crossings[0].lon;
		pass->ascending = ground->lat > 0;
	} else if ((fabs(_minus_180_to_180(crossings[1].lon - ground->lon)) <=
		    lon_tol) &&
		   (crossings[1].t > t)) {
		pass->t = crossings[1].t;
		pass->lon = crossings[1].lon;
		pass->ascending = ground->lat <= 0;
	}

	while (pass->t == 0) {
		int ret;

		double delta_lon_a =
			HUBBLE_TWO_PI_DEGREES -
			_zero_to_360(ground->lon + lon_tol - crossings[0].lon);
		double delta_lon_d =
			HUBBLE_TWO_PI_DEGREES -
			_zero_to_360(ground->lon + lon_tol - crossings[1].lon);

		if (delta_lon_a < delta_lon_d) {
			ret = _next_pass_get(orbit, true, delta_lon_a, lon_tol,
					     ground, crossings, pass, t);
			t = crossings[0].t;
		} else {
			ret = _next_pass_get(orbit, false, delta_lon_d, lon_tol,
					     ground, crossings, pass, t);
			t = crossings[1].t;
		}

		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}
