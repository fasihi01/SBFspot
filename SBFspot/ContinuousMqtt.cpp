/************************************************************************************************
                               ____  ____  _____                _
                              / ___|| __ )|  ___|__ _ __   ___ | |_
                              \___ \|  _ \| |_ / __| '_ \ / _ \| __|
                               ___) | |_) |  _|\__ \ |_) | (_) | |_
                              |____/|____/|_|  |___/ .__/ \___/ \__|
                                                   |_|

	SBFspot - Yet another tool to read power production of SMA® solar/battery inverters
	(c)2012-2019, SBF

	This is my hacky way to continuousely read inverter data and output it to a mqtt server.
    Don't use this. 
   
    All credits go to the original sbfspot creators, all bugs stay with me.

************************************************************************************************/
#include "ContinuousMqtt.h"
#include "version.h"
#include "osselect.h"
#include "endianness.h"
#include "SBFspot.h"
#include "misc.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include "bluetooth.h"
#include "Ethernet.h"
#include "SBFNet.h"
#include "sunrise_sunset.h"
#include "CSVexport.h"
#include "EventData.h"
#include "ArchData.h"
#include "SQLselect.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include "mqtt.h"
#include <signal.h>
#include <stdlib.h>

using namespace std;
using namespace boost;
using namespace boost::date_time;
using namespace boost::posix_time;
using namespace boost::gregorian;

Config* cfgLocal;
InverterData *InvertersLocal[10];

bool _interrupted = false;
void handleSigint(int signum)
{
    if (VERBOSE_NORMAL) puts("handle interrupt");
    _interrupted = true;

    if (cfgLocal->ConnectionType == CT_BLUETOOTH)
        logoffSMAInverter(InvertersLocal[0]);
    else
    {
        for (int inv = 0; InvertersLocal[inv] != NULL && inv < 10; inv++)
            logoffSMAInverter(InvertersLocal[inv]);
    }

    freemem(InvertersLocal);
    bthClose();
    if (VERBOSE_NORMAL) puts("handle interrupt done");
    exit(1);
}

int continuousMqtt(InverterData *Inverters[], Config *cfg)
{
    int rc = 0;

    cfgLocal = cfg;
    *InvertersLocal = *Inverters;

    signal(SIGINT, handleSigint);

    while (!_interrupted)
    {
        getInverterData(Inverters, EnergyProduction);
        if ((rc = getInverterData(Inverters, SpotDCPower)) != 0)
                std::cerr << "getSpotDCPower returned an error: " << rc << std::endl;

        if ((rc = getInverterData(Inverters, SpotDCVoltage)) != 0)
        {
                std::cerr << "getSpotDCVoltage returned an error: " << rc << std::endl;
                break;
        }

        if ((rc = getInverterData(Inverters, SpotACTotalPower)) != 0)
        {
                std::cerr << "getSpotDCVoltage returned an error: " << rc << std::endl;
                break;
        }

        //Calculate missing DC Spot Values
        CalcMissingSpot(Inverters[0]);

        rc = mqtt_publish(cfg, Inverters);

        if (rc != 0)
        {
                std::cout << "Error " << rc << " while publishing to MQTT Broker" << std::endl;
	            break;
        }
        sleep(5);
    }


	if (cfg->ConnectionType == CT_BLUETOOTH)
		logoffSMAInverter(Inverters[0]);
	else
	{
		for (int inv=0; Inverters[inv]!=NULL && inv<10; inv++)
			logoffSMAInverter(Inverters[inv]);
	}

    freemem(Inverters);
    bthClose();

	#if defined(USE_SQLITE) || defined(USE_MYSQL)
	if ((!cfg.nosql) && db.isopen())
		db.close();
	#endif


    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Done.\n");

    return 0;
}


