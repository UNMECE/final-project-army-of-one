#include "acequia_manager.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

static double computeSafeSurplus(Region* r)
{
    if (!r) return 0.0;

    double minLevelByNeed = 0.8 * r->waterNeed;
    double minLevelByCap  = 0.3 * r->waterCapacity;
    double minLevel       = std::max(minLevelByNeed, minLevelByCap);

    if (r->waterLevel <= minLevel) {
        return 0.0;
    }

    double keepLevel = std::max(minLevel, r->waterNeed);
    if (r->waterLevel <= keepLevel) {
        return 0.0;
    }

    return r->waterLevel - keepLevel;
}

static double computeDeficit(Region* r)
{
    if (!r) return 0.0;
    if (r->waterLevel >= r->waterNeed) {
        return 0.0;
    }
    return r->waterNeed - r->waterLevel;
}

//close all canals before deciding what to do this hour
static void closeAllCanals(const std::vector<Canal*>& canals)
{
    for (auto c : canals) {
        c->setFlowRate(0.0);
        c->toggleOpen(false);
    }
}

//configure a canal to move a desired amount of water in one hour
static void scheduleTransfer(Canal* canal, double amount)
{
    if (!canal || amount <= 0.0) return;

    // In Canal::updateWater:
    //   change += flowRate each second for 3600 seconds
    //   amountMoved = change / 1000 = (flowRate * 3600) / 1000 = 3.6 * flowRate
    // So: flowRate = amount / 3.6
    double flowRate = amount / 3.6;

    if (flowRate > 1.0) flowRate = 1.0;
    if (flowRate <= 0.0) return;

    canal->setFlowRate(flowRate);
    canal->toggleOpen(true);
}

void solveProblems(AcequiaManager& manager)
{
    const auto& regions = manager.getRegions();
    const auto& canals  = manager.getCanals();

    //check to see if the given scenario is even winnable from the start
    double totalWater = 0.0;
    double totalNeed  = 0.0;
    for (auto r : regions) {
        totalWater += r->waterLevel;
        totalNeed  += r->waterNeed;
    }

    if (totalWater < totalNeed) {
        std::cout << ">>> Scenario determined unwinnable based on initial conditions.\n";
        std::cout << ">>> Simulation will run, but a perfect solution is impossible.\n";
    }

    //identify the key regions and canals by name (as defined in AcequiaManager)
    Region* north = nullptr;
    Region* south = nullptr;
    Region* east  = nullptr;

    for (auto r : regions) {
        if      (r->name == "North") north = r;
        else if (r->name == "South") south = r;
        else if (r->name == "East")  east  = r;
    }

    Canal* canalA = nullptr; // North -> South
    Canal* canalB = nullptr; // South -> East
    Canal* canalC = nullptr; // North -> East
    Canal* canalD = nullptr; // East  -> North

    for (auto c : canals) {
        if      (c->name.find("A") != std::string::npos) canalA = c;
        else if (c->name.find("B") != std::string::npos) canalB = c;
        else if (c->name.find("C") != std::string::npos) canalC = c;
        else if (c->name.find("D") != std::string::npos) canalD = c;
    }

    //lambda to try moving water from src -> dst using a specific canal
    auto tryTransfer = [](Region* src, Region* dst, Canal* canal)
    {
        if (!src || !dst || !canal) return;

        double need    = computeDeficit(dst);
        double surplus = computeSafeSurplus(src);

        if (need <= 0.0 || surplus <= 0.0) {
            return;
        }

        //avoid overfilling destination (leave some safety margin)
        double maxBeforeFlood = dst->waterCapacity - dst->waterLevel;
        if (maxBeforeFlood <= 0.0) {
            return;
        }

        // Move at most:
        //  - what dst needs
        //  - what src can safely give
        //  - 80% of distance to capacity (to avoid floods)
        double amount = std::min({need, surplus, maxBeforeFlood * 0.8});
        if (amount <= 0.0) {
            return;
        }

        scheduleTransfer(canal, amount);
    };

    //adjust canals every simulated hour
    while (!manager.isSolved && manager.hour != manager.SimulationMax) {

        // Each hour, reset canals first
        closeAllCanals(canals);

        // PRIORITY relieve drought / big deficit regions if possible
        // Order: North<->South<->East according to the canal layout
        tryTransfer(north, south, canalA); // North -> South
        tryTransfer(north, east,  canalC); // North -> East
        tryTransfer(south, east,  canalB); // South -> East
        tryTransfer(east,  north, canalD); // East  -> North

        // Move time forward one hour; AcequiaManager will:
        //  - update canal water transfers
        //  - update region flags (flood / drought)
        //  - update penalties and isSolved
        manager.nexthour();
    }
}

