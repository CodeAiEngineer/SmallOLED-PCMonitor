/*
 * SmallOLED-PCMonitor - Metrics Display Module
 *
 * Functions for displaying PC stats metrics on the OLED display.
 */

#ifndef METRICS_H
#define METRICS_H

#include "../config/config.h"

// ========== Metrics Display Functions ==========

// Main display function - renders metrics grid
void displayStats();

// Compact grid layout with position-based rendering
void displayStatsCompactGrid();

// Helper to display a single metric
void displayMetricCompact(Metric* m);

// Draw progress bar for a metric
void drawProgressBar(int x, int y, int width, Metric* m);

// Overload alert: detects any visible % metric >=98 and shows a fullscreen
// alert (1s inverted + 3s normal = 4s total) with label and value.
// Returns true while an alert is active and should be drawn.
bool checkOverloadAlert();
void drawOverloadAlert();

#endif // METRICS_H
