#!/usr/bin/perl -w
#
# Copyright (c) 2012 Max Oberberger (max@oberbergers.de)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License 
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Version 1.6 (04.10.2012)
#
# cgi-bin Program receives every 5 minutes values from Arduino Weatherstation.
#
# Logfile is handled by logrotate. - /etc/logrotate.d/...
# Graph is in ~/temp/ of webserver user (apache: www-data - home=/var/www)
##########

use strict;
use RRD::Simple (); # Round Robin Database simple
use CGI::Carp qw( fatalsToBrowser );
use CGI qw/:standard/;
## Logfile
use Log::Dispatch::File;

##################################################
###########          variables       #############
##################################################
my ($home) = glob "~";
my $dbFolder = "$home/arduino/databases";
my $arduinoFolder = "$home/arduino";
my $logfile = "/var/log/arduino/Wetterstation.log";
### Database
my $dbSimple = "$home/arduino/databases/weatherstationSimple.rrd";
my $rrdSimple = "";

### Sensoren
my $temperature;
my $pressure;
my $humidity;

########## Time - maybe complicated but it works ;)
## to show a temp and pressure graph of 24 hours
my $cur_time = time();                # set current time
my $start_time = $cur_time - 43200;   # set start time to 12 hours behind current time (24h = 86400 [60*60*24])
my ($Sekunden, $Minuten, $Stunden, $Monatstag, $Monat, $Jahr, $Wochentag, $Jahrestag, $Sommerzeit) = localtime(time);

$Monat = $Monat < 10 ? $Monat= "0".$Monat : $Monat;
$Monatstag = $Monatstag < 10 ? $Monatstag= "0".$Monatstag : $Monatstag;
$Stunden = $Stunden < 10 ? $Stunden= "0".$Stunden : $Stunden;
$Minuten = $Minuten < 10 ? $Minuten = "0".$Minuten : $Minuten;
$Sekunden = $Sekunden < 10 ? $Sekunden = "0".$Sekunden : $Sekunden;
$Jahr += 1900;

## time for logfile
my $zeit = "####### $Monatstag.$Monat.$Jahr - $Stunden:$Minuten.$Sekunden #######\n";

open(LOG, ">>$logfile") || die "Error to open logfile\n";
print LOG "$zeit";
print LOG "++ program was called from arduino\n";

## check if dbFolder exists
## if not, create it
if(! -d $arduinoFolder){
	mkdir $arduinoFolder or die print LOG "-- error with mkdir $arduinoFolder\n";
	if(! -d $dbFolder){
		mkdir $dbFolder or die print LOG "-- error with mkdir $dbFolder\n";
	}
}

createRRDdatabase();
	
############ read Data from Arduino
my $Daten = "";
if($ENV{'REQUEST_METHOD'} eq 'POST') {
	$Daten = $ENV{'QUERY_STRING'};
	print LOG "++ POST-Request called\n";
	extractSensorValues();
	updateRRDdatabase();
	graphRRDdata();
}else {
	read(STDIN, $Daten, $ENV{'CONTENT_LENGTH'});
}

print LOG "++ End of program\n\n";

close (LOG);

sub extractSensorValues {
	### split values in pressure and temperature
	### Uebergabeformat: P=ZAHL&T=ZAHL
	my @ValueField = split(/&/, $Daten);
	foreach my $Field (@ValueField) {
		(my $name,my $value) = split(/=/, $Field);
		if ($name eq "P" ){  # pressure
	        	$pressure = $value / 100.0; # get pressure in hPa and not in Pa
			print LOG "++ got pressure value from arduino: $value\n";
		}elsif ( $name eq "T" ){ # Temperature
			$temperature = $value;
			print LOG "++ got temperature value from arduino: $value\n";
		}elsif ( $name eq "H" ){ # Humidity
			$humidity = $value;
			print LOG "++ got humidity value from arduino: $value\n";
		}
		if(!defined($name)){
			print LOG "-- error with webcall of arduino.pl. Nothing contains T,H,P\n";
		}
		if(!defined($value)){
			print LOG "-- error with webcall of arduino.pl. No values contained\n";
		}
	}
}

sub updateRRDdatabase {
	$rrdSimple->update(
		temperature => $temperature,
		pressure => $pressure,
		humidity => $humidity
		);
}

sub graphRRDdata {
	my %rtn = $rrdSimple->graph(
		destination => "$home/temp",
		title => "Wetterstation",
		width => "500",
		height => "200",
		vertical_label => "Temperatur/Druck/Humidity",
		interlaced => ""
		);
}

sub createRRDdatabase {
	$rrdSimple = RRD::Simple->new( file => $dbSimple );
	if(! -f $dbSimple){
		$rrdSimple->create(
			temperature => "GAUGE",
			pressure => "GAUGE",
			humidity => "GAUGE"
		);
	}
}
################### END arduino.pl
