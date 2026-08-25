#ifndef BROWNIAN_MOTOR_SIMULATIONOBSERVER_H
#define BROWNIAN_MOTOR_SIMULATIONOBSERVER_H
#pragma once

#include <functional>

#include "SimulationUpdate.h"

using SimulationObserver = std::function<void(const SimulationUpdate&)>;

#endif // BROWNIAN_MOTOR_SIMULATIONOBSERVER_H