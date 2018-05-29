/* intronProspector.cc - command line program to extract 

    Copyright (c) 2018, Mark Diekhans, University of California, Santa Cruz

This code is derived from the regtools package available at:

  https://github.com/griffithlab/regtools

We thank them for providing this software under a flexible license:

    Copyright (c) 2015, The Griffith Lab

    Author: Avinash Ramu <aramu@genome.wustl.edu>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.  */

#include <getopt.h>
#include <algorithm>
#include <fstream>
#include "junctions_extractor.hh"

using namespace std;

static const uint32_t DEFAULT_MIN_ANCHOR_LENGTH = 8;
static const uint32_t DEFAULT_MIN_INTRON_LENGTH = 70;
static const uint32_t DEFAULT_MAX_INTRON_LENGTH = 500000;
static const unsigned DEFAULT_STRANDED = JunctionsExtractor::UNSTRANDED;

static const char *usage_msg =
#include "manpage.h"
    ;

// Usage statement for this tool and exit non-zero.
static void usage() {
    cerr << usage_msg;
    exit(1);
}

// convert a specification for strandness to constant.
static int str_to_strandness(const string s) {
    // case-insensitive compare
    string su(s);
    transform(su.begin(), su.end(), su.begin(), ::toupper);
    if (su == "UN") {
        return JunctionsExtractor::UNSTRANDED;
    } else if (su == "RF") {
        return JunctionsExtractor::RF_STRANDED;
    } else if (su == "FR") {
        return JunctionsExtractor::FR_STRANDED;
    } else {
        throw runtime_error("invalid strandness value '" + s + "', expected one of 'UN', 'RF', or 'FR' (case insensitive)");
    }
}


// Parse command line
class CmdParser {
    public:
    // program info
    int print_help;
    int print_version;

    // input
    string bam_file;
    uint32_t min_anchor_length;
    uint32_t min_intron_length;
    uint32_t max_intron_length;
    int strandness;

    // output
    string junction_bed;
    string intron_bed;

    CmdParser(int argc, char *argv[]):
        print_help(false),
        print_version(false),
        bam_file("/dev/stdin"),
        min_anchor_length(DEFAULT_MIN_ANCHOR_LENGTH),
        min_intron_length(DEFAULT_MIN_INTRON_LENGTH),
        max_intron_length(DEFAULT_MAX_INTRON_LENGTH),
        strandness(JunctionsExtractor::UNSTRANDED) {

        struct option long_options[] = {
            {"help", no_argument, &print_help, 1},
            {"version", no_argument, &print_version, 1},
            {"min-anchor-length", required_argument, NULL, 'a'},
            {"min-intron_length", required_argument, NULL, 'i'},
            {"max-intron_length", required_argument, NULL, 'I'},
            {"strandness", required_argument, NULL, 's'},
            {"junction-bed", required_argument, NULL, 'j'},
            {"intron-bed", required_argument, NULL, 'n'},
            {NULL, 0, NULL, 0}
        };
            
        const char *short_options = "hva:i:I:s:j:n:";
        int c;
        while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
            switch (c) {
                case 'a':
                    min_anchor_length = atoi(optarg);
                    break;
                case 'i':
                    min_intron_length = atoi(optarg);
                    break;
                case 'I':
                    max_intron_length = atoi(optarg);
                    break;
                case 's':
                    strandness = str_to_strandness(optarg);
                    break;
                case 'j':
                    junction_bed = optarg;
                    break;
                case 'n':
                    intron_bed = optarg;
                    break;
                case 'h':
                    usage();
                case '?':
                default:
                    cerr << "Invalid option '" << argv[optind] << "'" << endl;
                    usage();
            }
        }

        if (argc - optind > 1) {
            cerr << "Error: too many position arguments" << endl << endl;
            usage();
        }
        if (argc - optind == 1) {
            bam_file = string(argv[optind++]);
        }
        if (print_help) {
            usage();
        }
    }
};

// Print BED with anchors as blocks and intron as gap.
static void print_anchor_bed(const vector<Junction*>& juncs,
                             const string& outfile) {
    ofstream out(outfile);
    for (vector<Junction*>::const_iterator it = juncs.begin(); it != juncs.end(); it++) {
        (*it)->print_anchor_bed(out);
    }
}

// Print BED with intron as block
static void print_intron_bed(const vector<Junction*>& juncs,
                             const string& outfile) {
    ofstream out(outfile);
    for (vector<Junction*>::const_iterator it = juncs.begin(); it != juncs.end(); it++) {
        (*it)->print_intron_bed(out);
    }
}

// entry point
int main(int argc, char *argv[]) {
    CmdParser opts(argc, argv);
    JunctionsExtractor je(opts.bam_file,
                          opts.min_anchor_length,
                          opts.min_intron_length, opts.max_intron_length,
                          opts.strandness);
    je.identify_junctions_from_bam();
    vector<Junction*> juncs = je.get_junctions_sorted();
    if (opts.junction_bed != "") {
        print_anchor_bed(juncs, opts.junction_bed);
    }
    if (opts.intron_bed != "") {
        print_intron_bed(juncs, opts.intron_bed);
    }
    return 0;
}