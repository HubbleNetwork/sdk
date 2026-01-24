/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ephemeris.h
 * @brief Hubble Network Satellite Ephemeris APIs
 **/

#ifndef INCLUDE_HUBBLE_SAT_EPHEMERIS_H
#define INCLUDE_HUBBLE_SAT_EPHEMERIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct orbit_info
 * @brief Represents the orbital information of a satellite.
 *
 * This structure contains the parameters required to describe the orbit
 * of a satellite, including its position, motion, and orientation.
 */
struct orbit_info {
	/** Reference epoch time (seconds since Unix epoch) */
	uint64_t t0;
	/** Mean motion at epoch (radians per second) */
	double n0;
	/** Rate of change of mean motion (radians per second^2) */
	double ndot;
	/** Right ascension of ascending node at epoch (radians) */
	double raan0;
	/** Rate of change of RAAN (radians per second) */
	double raandot;
	/** Argument of perigee at epoch (radians) */
	double aop0;
	/** Rate of change of argument of perigee (radians per second) */
	double aopdot;
	/** Inclination (degrees) */
	double inclination;
	/** Eccentricity (unitless, 0=circular) */
	double eccentricity;
	/** NORAD ID of the satellite */
	uint32_t sat_id;
};

/**
 * @struct ground_info
 * @brief Represents the location of a ground station.
 *
 * This structure contains the latitude and longitude of a
 * ground station on Earth.
 */
struct ground_info {
	/** Latitude in degrees. */
	double lat;
	/** Longitude in degrees. */
	double lon;
};

/**
 * @struct hubble_pass_info
 * @brief Represents information about a satellite pass.
 *
 * This structure contains details about a satellite's pass over a
 * specific location, including the time, longitude, and whether the
 * satellite is ascending or descending.
 */
struct hubble_pass_info {
	/** Longitude of the satellite pass (degrees, East positive). */
	double lon;
	/** Time of the satellite pass (Unix time, seconds since epoch). */
	uint64_t t;
	/** True if the satellite is ascending (moving northward), false if descending. */
	bool ascending;
};

/**
 * @brief Set the orbital information for one or more satellites.
 *
 * This function stores orbital parameters for multiple satellites.
 * If orbit information already exists for a satellite, it is overwritten
 * with the new values provided.
 *
 * @param orbits Pointer to the array of orbital parameters to be set.
 * @param count Number of entries in the orbital parameters array.
 * @return 0 on success or a negative value in case of error.
 */
int hubble_orbit_info_set(const struct orbit_info *orbits, size_t count);

/**
 * @brief Clear all orbital information.
 *
 * This function clears all stored orbital parameters for all satellites.
 * @return 0 on success, or a negative value in case of error.
 */
int hubble_orbit_info_clear(void);

/**
 * @brief Get the next satellite pass.
 *
 * This function calculates the next satellite pass over a given
 * ground station, based on the orbit information previously
 * provided using the @ref hubble_orbit_info_set function and the
 * ground station's location.
 *
 * @param t Current time or the time from which to start the calculation.
 * @param ground Pointer to the ground station's location.
 * @param pass The next satellite pass in case of success.
 * @return 0 on success,
 *         -ENOENT if no orbit info has been provisioned for any satellite,
 *         or a negative value in case of error.
 */
int hubble_next_pass_get(uint64_t t, const struct ground_info *ground,
			 struct hubble_pass_info *pass);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_SAT_EPHEMERIS_H */
