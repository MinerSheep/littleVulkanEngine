#pragma once

class HttpClient; // defined in http_client.hpp; only referenced by ref here

// Land/sea test for a geographic coordinate. Returns true if the point is over
// land, false if over open water.
//
// Backed by the open-meteo elevation API: the Copernicus DEM reports 0 m over
// ocean, so any coordinate above sea level is treated as land. This is a
// coastline-fuzzy heuristic -- below-sea-level land and 0 m coastal land can
// misclassify -- but it needs no bundled dataset and reuses the existing
// HttpClient / open-meteo plumbing.
//
// A failed lookup (bad coordinate, network error, malformed reply) returns
// false, i.e. "open water": a transient failure won't conjure phantom land that
// blocks navigation. Intended for the planned ground-collision check in the
// ServerNav sim.
bool isLand(float lat, float lon);

// Same, but against a caller-owned HttpClient so its lifetime/connection reuse
// can be shared with fetchWeather and other lookups. See fetch_weather.hpp.
bool isLand(HttpClient& http, float lat, float lon);
