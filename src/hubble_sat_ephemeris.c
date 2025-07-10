#include <hubble_sat_ephemeris.h>

#include <math.h>
#include <string.h>
#include <errno.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define HUBBLE_EARTH_RADIUS        6378136.999954619 /* Earth radius at equator */
#define HUBBLE_EARTH_ROTATION_RATE 7.292115855377074e-05 /* rad / s */
#define HUBBLE_TEME_REF_DATETIME_2027                                          \
	1798761600
#define HUBBLE_TEME_ANGLE_2027           1.7526971469712507
#define HUBBLE_TWO_PI_DEGREES            360
#define HUBBLE_PI_DEGREES                180
#define HUBBLE_ELEVATION_ANGLE_TOLERANCE 30
#define HUBBLE_SAT_ELEVATION             6892550.590445475

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

static double _signed_fmod(double x, double y)
{
	double ret;

	if (y == 0.0) {
		return NAN;
	}

	ret = fmod(x, y);
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

	return fmod(angle, (2.00 * M_PI));
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

	E = 2 * atan(sqrt((1 - e) / (1 + e)) * tan(theta / 2));
	me = E - e * sin(E);

	return _zero_to_2pi(me);
}

/* Gets the time of the ascending node for a given orbit count */
static uint64_t _anode_time_get(const struct orbit_info *info, int count)
{
	double dt;

	if (info->ndot == 0.0) {
		dt = count / info->n0;
	} else {
		dt = (sqrt(info->n0 * info->n0 + 2 * info->ndot * count) -
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

	if (fabs(sin(inclination)) <= fabs(sin(latrad))) {
		return -1;
	}

	anode_time = _anode_time_get(orbit, orbit_count);
	dt_anode = (int64_t)anode_time - orbit->t0;
	raan = orbit->raan0 + (orbit->raandot * dt_anode);
	aop = orbit->aop0 + (orbit->aopdot * dt_anode);
	orbit_period = 1.0 / (orbit->n0 + (orbit->ndot * dt_anode));
	if (latrad >= 0) {
		ra1 = raan + asin(tan(latrad) / tan(inclination));
		ra2 = raan + M_PI - asin(tan(latrad) / tan(inclination));
	} else {
		ra2 = raan + asin(tan(latrad) / tan(inclination));
		ra1 = raan + M_PI - asin(tan(latrad) / tan(inclination));
	}

	if (latrad >= 0) {
		lam1 = asin(sin(latrad) / sin(inclination));
		lam2 = M_PI - lam1;
	} else {
		lam1 = M_PI - asin(sin(latrad) / sin(inclination));
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

	C = asin(earth.radius * sin(A) / HUBBLE_SAT_ELEVATION);
	b = earth.radius * cos(M_PI - asin(HUBBLE_SAT_ELEVATION *
					     (sin(C) / earth.radius))) +
	    (HUBBLE_SAT_ELEVATION * (cos(C)));
	B = asin(b * sin(C) / earth.radius);

	return _RAD2DEG(asin((earth.radius * sin(B)) /
			      (earth.radius * cos(_DEG2RAD(lat)))));
}

/*
 * Helper function to calculate the next pass for ascending or
 * descending crossings
 */
static int _next_pass_get(
	const struct orbit_info *orbit, bool ascending, double delta_lon,
	double lon_tol, const struct ground_info *ground,
	struct crossing_info crossings[2], struct hubble_pass_info *pass,
	uint64_t t)

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
	} else if ((fabs(_minus_180_to_180(crossings[1].lon - ground->lon)) <= lon_tol) &&
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
