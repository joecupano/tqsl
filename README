This is the TrustedQSL project, which provides tools for digitally
signing Amateur Radio QSO records.

src: Source code and documentation for tqsllib, the TrustedQSL
     library.

apps: Source code for "tqsl" and other sample applications.

html: Various legacy documents

This document describes the changes to TrustedQSL since version 1.13 and
explains how applications can use TQSL in their applications.

Command Line Changes
--------------------
Many applications use the 'tqsl' application in command line mode to sign
log files. There were several new capabilities added to command line operation
in 1.14. The first is that tqsl can now automatically sign and upload a log to
the LoTW site for the user. This allows your application to simply write
an adif file which is then processed and uploaded to LoTW without requiring
the application to read the output file and either upload it or tell your user
to upload it.

The command line parser in 1.14 was rewritten and is less
forgiving of improperly formatted command lines. 

Command Line Options
--------------------
The following summarizes the command line options and what they do:
Usage: tqsl [-a <str>] [-b <date> ] [-e <date>] [-d] [-l <str>] [-s]
            [-f <str>] [-o <str>] [-u] [-x] [-p <str>] [-q] [-n] [-v] [-h]
            [-t <file>] [-c <call> ] [Input ADIF or Cabrillo log file to sign]


The following command line options may be specified on the command line:
  -a <str>      Specify dialog action - abort, all, compliant or ask

This option instructs TQSL on how to handle QSOs that do not appear to be
valid. There are many potential causes for invalid QSOs. Examples include QSOs
with dates outside the valid range for the certificate being used, QSOs
with invalid amateur callsigns, duplicate QSOs, and attempts to sign with an
expired certificate.

This option specifies how tqsl should handle these exceptions. Using "-a ask"
instructs tqsl to use a dialog to ask the user how to proceed. This is the
default behavior if "-a" is not provided on the command line. 

Using "-a abort" instructs tqsl to issue an error message when an exception
QSO is processed and immediately abort signing.

Using "-a compliant" instructs tqsl to sign the QSOs which are compliant
(not duplicates, in date range, and with valid callsigns) and ignore any
exception QSOs. This is the recommended behavior for command line applications
but is not the default action for compatibility reasons.

Using "-a all" instructs tqsl to process all QSOs, ignoring duplicates and
invalid callsigns. QSOs outside the range of valid dates for the selected
station certificate will not be signed, as they would not be accepted by
Logbook.


  -b            Set begin date for the QSO date filter.
  -e            Set end date for the QSL date filter.

These options filter QSOs being signed to those after the begin date and
before the end date. If neither of these are supplied, then no filtering
will be performed.  These values will override any date range entries
provided by the user.  This implies that "-d" (suppress date range dialog)
should be used with -b or -e.

  -d            Suppress date range dialog

This option instructs tqsl to not ask the user to select a range of dates
for processing QSOs. If this is used, all QSOs in the input file will be
selected for processing. Command line tools will usually include this
option to suppress tqsl dialogs. However, this means that the logging
program is responsible for filtering QSOs before delivering them to tqsl.

  -f <str>      Select handling option for QTH details

This option instructs tqsl on how to handle the MY_ fields in an
ADIF log file. These specify the callsign, QTH details such as zones,
gridsquare, state/county, etc. for a QSO. If the log includes this
detail, TQSL can read these fields and take action to use that
information to ensure that the Station Location is correct. By
default, TQSL will report any discrepancies between QTH and the
Station Location; TQSL can also be directed to ignore the QTH
information.

  -f ignore     Directs TQSL to ignore the QTH information
  -f report     Directs TQSL to report any differences between the
                station location and the MY_ QTH deails
  -f update     Directs TQSL to overwrite the Station Location
                information with data from the log.

When using "-f update", the ideal practice should be to either
specify all QTH information (state, county, grid, zones) in the
ADIF "MY_" fields, or use an minimal Station Location with no
QTH data.

  -l <str>      Select Station Location.

This option selects a station location. This is used for signing logs or
in conjunction with the "-s" option to define a location for editing.

  -s            Edit (if used with -l) or create Station Location

This option can be used to create a new Station Location (-s without -l) or
to edit an existing Station Location (when both -s and -l are provided).

  -o <str>      Output file name (defaults to input name minus extension
                plus .tq8)

This option instructs tqsl where the signed output file will be stored. If it
is not provided, the output file will be written to the same location as the
input file with the extension changed to ".tq8"

  -u            Upload after signing instead of saving

This option instructs tqsl to upload the log file after it is successfully
signed.

  -q            Operate in batch mode, not menu-driven mode.
  -x            Operate in batch mode, not menu-driven mode.

If -x or -q are included on the command line, tqsl suppresses user dialogs
and sends error messages to standard error. A logging application is expected
to read this file and possibly display the contents to the user so they
can see the results of the command action. If these options are not included,
a calling application cannot distinguish between a successful signing and
one where a user cancels the signing.

  -n            Look for updated (new) versions of key files.

If -n is given on the command line, tqsl checks for new versions of
the tqsl program, an updated tqsl configuration file, and verifies that any
user certificates are not about to expire. If any of these circumstances
exist, the user is prompted to perform the required updates. When the check is
completed, tqsl exits. This command line option can not be combined with any
other command line options as it only performs an update check and does not
sign any logs submitted with the command.

  -p <str>      Password for the signing key

This option allows an application to provide the password for the private key
that will be used to sign the log file.

  -v            Display the version information and exit
  -h            Display command line help

These options allow the user to display the version number of tqsl or to
obtain help on the command line usage.

  -t <file>     Open a diagnostic trace file at startup

When supplied, this option enables diagnostic tracing at startup, opening
the supplied file to record TQSL operations details. This is useful for
debugging purposes.

  -c <call>     Use the given callsign when signing a log

This option allows a logging program to specify what callsign to use for a 
log signing operation. This will override the callsign associated with the
selected station location, if any.

Command Line Usage
------------------
An application that uses the command line invokes the tqsl binary, optionally
providing a set of options that dictate how tqsl operates. Normally, such
an application should include the "-x" or "-q" options to indicate to tqsl
that application popups should be suppressed.

Errors discovered during the signing process are sent to the standard error
file. Callers would normally indicate where those messages should be sent
by adding "2> file.txt" to the command line used to run tqsl. This directs
the shell (Windows or Unix) to write the error messages to that file.

When operated in "batch" mode (i.e. -x or -q used), tqsl provides information
that the calling program can use to determine if the signing operation
succeeded. The first way is by capturing tqsl's exit status code. This
provides information on success or failure using the following values:

        0 - Success: all QSOs submitted were signed and saved or uploaded
        1 - Cancelled by user
        2 - The log was rejected by the LoTW server
        3 - The response from the LoTW server was unexpected
        4 - An error occurred in tqsl
        5 - An error occurred in tqsllib (invalid filename, bad file format)
        6 - Unable to open input file
        7 - Unable to open output file
        8 - No QSOs were processed because some QSOs were duplicates or
            out of date range (no QSOs written)
        9 - Some QSOs were processed, and some QSOs were ignored because
            they were duplicates or out of date range (some QSOs written)
       10 - Command syntax error
       11 - LoTW network connection failed (no network or LoTW is unreachable)
       12 - Unknown error
       13 - The TQSL duplicates database is locked.

This exit status is also written to stderr in a format that can be parsed
by the calling application. The last output from tqsl will be of the format
hh:mm:ss AM|PM Final Status: Description (code) 
(For cases where the language is not English, this will be duplicated - first
in the local language, then in English.)

The first two fields are a timestamp, the words "Final Status:" always appears.
Following that is a short descriptive message giving the exit status. The
last thing on the line is the numeric exit code (as above) in parenthesis.

Examples of output follows:
05:57:39 PM: Warning: Signing cancelled
05:57:39 PM: No records output
05:57:39 PM: Final Status: cancelled by user (1)

06:05:56 PM: /home/rmurphy/k1mu.adi: 414 QSO records were duplicates
06:05:56 PM: /home/rmurphy/k1mu.adi: wrote 1 records to /home/rmurphy/k1mu.tq8
06:05:56 PM: /home/rmurphy/k1mu.tq8 is ready to be emailed or uploaded.
Note: TQSL assumes that this file will be uploaded to LoTW.
Resubmitting these QSOs will cause them to be reported as duplicates.
06:05:56 PM: Final Status: Some QSOs were duplicates or out of date range (9)

An example usage for signing a log would be

tqsl -q -l "K1MU home" -p "Insecure" -a compliant -u -d k1mu.adi 2>temp.txt

This indicates quiet mode (-q), selects a station location and a password,
indicates that only compliant QSOs will be written (-a), uploads to LoTW (-u),
suppresses date popups (-d), provides an input file (k1mu.adi), and finally
writes log messages to temp.txt. The logging program would read and process
that log once tqsl is done. An application would add "-o" to indicate where
tqsl should write the signed log if "-u" (upload) is not provided.

Command line applications are strongly encouraged to add "-a=compliant" to
their invocations of tqsl, and to consider storing and displaying the log
messages to their users.

Application Changes
-------------------
Some logging applications directly call tqsllib functions to sign log files.
The application programming interface (API) to tqsllib has not changed in
ways that introduce incompatibilities, but there are additional API calls
which are necessary for applications to allow duplicate QSO processing to
work properly.

Normally, an application will call tqsl_beginCabrilloConverter() or
tqsl_beginADIFConverter to begin signing a log file. After the converter
is created by those calls, the application should then call
        tqsl_setConverterAllowDuplicates(conv, false)
which tells tqsllib that duplicate processing should be enabled. If you do
not call tqsl_setConverterAllowDuplicates, the library will assume that
duplicates should be permitted (for compatibility reasons), which may cause
unnecessary QSOs to be uploaded.

If duplicate suppression is enabled, there is a new error return from 
tqsl_getConverterGABBI that indicates duplicate QSOs. In this case, 
tQSL_Error is set to TQSL_DUPLICATE_QSO. Software may need to be modified
to handle this new result and act appropriately (ignore it, or abort the
signing operation.)

After successful processing of a log, an application should call either
tqsl_convertCommit(conv) or tqsl_convertRollBack(conv) prior to calling
tqsl_endConverter() to signal that a log conversion has completed.
tqsl_converterCommit() indicates to tqsllib that the log has been successfully
processed and that the QSOs should be added to the duplicate detection
database. Calling tqsl_converterRollBack() indicates to tqsllib that the
log has not been successfully processed and that the QSO records should
not be added to the duplicate database. Simply adding the necessary call
before the converter is closed is enough to bring the application up to date.

change
        tqsl_endConverter(&conv)
to
        tqsl_converterCommit(conv);
        tqsl_endConverter(&conv);

Using tqsllib
-------------
A minimal set of calls to permit an application to sign a log is the following.
Of course, error checking should be performed for each call.

        tqsl_getStationLocation(&loc, location_name);

        tqsl_getLocationCallSign(loc, callsign, sizeof callsign);
        tqsl_getLocationDXCCEntity(loc, &dxcc);
        tqsl_selectCertificates(&certlist, &ncerts, callsign, dxcc);

        tqsl_beginADIFConverter(&conv, input_file, certlist, ncerts, loc);
        tqsl_setConverterAllowDuplicates(conv, false);

        tqsl_setConverterAppName(conv, "myAppName");
            (tell tqsllib the name of your application)

        while (cp = tqsl_getConverterGABBI(conv) != 0)
            write the string pointed to by "cp" to your file

        tqsl_converterCommit(conv);
        tqsl_endConverter(&conv);
        tqsl_endStationLocationCapture(&loc);

The tq8 files created by tqsl are compressed using zlib functions. You can
also submit uncompressed files using a .tq7 extension.
