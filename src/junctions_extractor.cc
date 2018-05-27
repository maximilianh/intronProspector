/*  junctions_extractor.cc -- splice junction extraction.

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

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "junctions_extractor.hh"
#include "htslib/sam.h"
#include "htslib/hts.h"
#include "htslib/faidx.h"
#include "htslib/kstring.h"

using namespace std;

// convert a char to a string
static string char_to_string(char ch) {
    char chs [2] = {ch, '\0'};
    return string(chs);
}

// convert a unsigned integer to a string
static string uint32_to_string(uint32_t num) {
    stringstream s1;
    s1 << num;
    return s1.str();
}

// Do some basic qc on the junction
bool JunctionsExtractor::junction_qc(uint32_t anchor_start, uint32_t anchor_end,
                                     uint32_t thick_start, uint32_t thick_end) {
    if ((anchor_end - anchor_start < min_intron_length_) ||
        (anchor_end - anchor_start > max_intron_length_)) {
        return false;
    } else if ((anchor_start - thick_start < min_anchor_length_) ||
               (thick_end - anchor_end < min_anchor_length_)) {
        return false;
    } else {
        return true;
    }
}

// make key for a junction
string JunctionsExtractor::make_junction_key(const string& chrom, char strand,
                                             uint32_t start, uint32_t end) {
    // since ?,+,- sort differently on different systems
    string strand_proxy;
    if (strand == '+') {
        strand_proxy = "0";
    } else if (strand == '-') {
        strand_proxy = "1";
    } else {
        strand_proxy = "2";
    }
    return chrom + string(":") + uint32_to_string(start) + "-" + uint32_to_string(end) + ":" + strand_proxy;
}

// Add a junction to the junctions map
// The read_count field is the number of reads supporting the junction.
void JunctionsExtractor::add_junction(const string& chrom, char strand,
                                      uint32_t anchor_start, uint32_t anchor_end,
                                      uint32_t thick_start, uint32_t thick_end) {
    // Check junction_qc
    if (!junction_qc(anchor_start, anchor_end, thick_start, thick_end)) {
        return;
    }

    // Construct key chr:start-end:strand
    string key = make_junction_key(chrom, strand, thick_start, thick_end);

    // Check if new junction
    Junction *junc = NULL;
    if (!junctions_.count(key)) {
        junc = new Junction(chrom, anchor_start, anchor_end,
                            thick_start, thick_end, strand);
        junctions_[key] = junc;
    } else {
         // existing junction
        junc = junctions_[key];
        // Check if thick starts are any better
        if (thick_start < junc->thick_start)
            junc->thick_start = thick_start;
        if (thick_end > junc->thick_end)
            junc->thick_end = thick_end;
    }
    junc->read_count += 1;
}

#if 0 // FIXME
// Print all the junctions - this function needs work
vector<Junction> JunctionsExtractor::get_all_junctions() {
    return junctions_vector_;
}

// Print all the junctions - this function needs work
void JunctionsExtractor::print_all_junctions(ostream& out) {
    ofstream fout;
    if (output_file_ != string("NA")) {
        fout.open(output_file_.c_str());
    }
    // Sort junctions by position
    if (!junctions_sorted_) {
        create_junctions_vector();
        sort_junctions(junctions_vector_);
        junctions_sorted_ = true;
    }
    for (vector<Junction> :: iterator it = junctions_vector_.begin();
        it != junctions_vector_.end(); it++) {
        Junction j1 = *it;
        if (j1.has_left_min_anchor && j1.has_right_min_anchor) {
            if (fout.is_open())
                j1.print(fout);
            else
                j1.print(out);
        }
    }
    if (fout.is_open())
        fout.close();
}
#else
// Print all the junctions - this function needs work
void JunctionsExtractor::print_all_junctions(ostream& out) {
    for (map<string, Junction*>::iterator it = junctions_.begin(); it != junctions_.end(); it++) {
        Junction *junc = it->second;
        junc->print(out);
    }
}
#endif

// Get the strand from the XS aux tag
char JunctionsExtractor::get_junction_strand_XS(bam1_t *aln) {
    uint8_t *p = bam_aux_get(aln, "XS");
    if (p != NULL) {
        char strand = bam_aux2A(p);
        return strand ? strand : '.';
    } else {
        return '.';
    }
}

// Get the strand from the bitwise flag
char JunctionsExtractor::get_junction_strand_flag(bam1_t *aln) {
    uint32_t flag = (aln->core).flag;
    int reversed = bam_is_rev(aln);
    int mate_reversed = bam_is_mrev(aln);
    int first_in_pair = (flag & BAM_FREAD1) != 0;
    int second_in_pair = (flag & BAM_FREAD2) != 0;
    // strandness_ is 0 for unstranded, 1 for RF, and 2 for FR
    int bool_strandness = strandness_ - 1;
    int first_strand = !bool_strandness ^ first_in_pair ^ reversed;
    int second_strand = !bool_strandness ^ second_in_pair ^ mate_reversed;
    char strand;
    if (first_strand){
        strand = '+';
    } else {
        strand = '-';
    }
    // if strand inferences from first and second in pair don't agree, we've got a problem
    if (first_strand == second_strand){
        return strand;
    } else {
        return '.';
    }
}

// Get the strand
char JunctionsExtractor::get_junction_strand(bam1_t *aln) {
    if (strandness_ != UNSTRANDED){
        return get_junction_strand_flag(aln);
    } else {
        return get_junction_strand_XS(aln);
    }
}

// Parse junctions from the read and store in junction map
int JunctionsExtractor::parse_alignment_into_junctions(bam_hdr_t *header, bam1_t *aln) {
    int n_cigar = aln->core.n_cigar;
    if (n_cigar <= 1) // max one cigar operation exists(likely all matches)
        return 0;

    const string& chrom = targets_[aln->core.tid];
    char strand = get_junction_strand(aln);
    int read_pos = aln->core.pos;
    uint32_t *cigar = bam_get_cigar(aln);

    uint32_t anchor_start = read_pos;
    uint32_t anchor_end = 0;
    uint32_t thick_start = read_pos;
    uint32_t thick_end = 0;
    bool started_junction = false;
    for (int i = 0; i < n_cigar; ++i) {
        char op = bam_cigar_opchr(cigar[i]);
        int len = bam_cigar_oplen(cigar[i]);
        switch(op) {
            case 'N': // skipped region from the reference
                if (!started_junction) {
                    anchor_end = anchor_start + len;
                    thick_end = anchor_end;
                    // Start the first one and remains started
                    started_junction = true;
                } else {
                    // Add the previous junction
                    add_junction(chrom, strand, anchor_start, anchor_end, thick_start, thick_end);
                    thick_start = anchor_end;
                    anchor_start = thick_end;
                    anchor_end = anchor_start + len;
                    thick_end = anchor_end;
                    // For clarity - the next junction is now open
                    started_junction = true;
                }
                break;
            case '=':  // sequence match
            case 'M':  // alignment match (can be a sequence match or mismatch)
                if (!started_junction)
                    anchor_start += len;
                else
                    thick_end += len;
                break;
            // No mismatches allowed in anchor
            case 'D':  // deletion from the reference
            case 'X':  // sequence mismatch
                // FIXME: do we want to keep this restriction?
                if (!started_junction) {
                    anchor_start += len;
                    thick_start = anchor_start;
                } else {
                    add_junction(chrom, strand, anchor_start, anchor_end, thick_start, thick_end);
                    // Don't include these in the next anchor
                    anchor_start = thick_end + len;
                    thick_start = anchor_start;
                }
                started_junction = false;
                break;
            case 'I':  // insertion to the reference
            case 'S':  // soft clipping (clipped sequences present in SEQ)
                if (!started_junction)
                    thick_start = anchor_start;
                else {
                    add_junction(chrom, strand, anchor_start, anchor_end, thick_start, thick_end);
                    // Don't include these in the next anchor
                    anchor_start = thick_end;
                    thick_start = anchor_start;
                }
                started_junction = false;
                break;
            case 'H':  // hard clipping (clipped sequences NOT present in SEQ)
                break;
            default:
                throw new std::invalid_argument("Unknown cigar operation '" + char_to_string(op) + "' found in " + bam_);
        }
    }
    if (started_junction) {
        add_junction(chrom, strand, anchor_start, anchor_end, thick_start, thick_end);
    }
    return 0;
}

// build target array from bam header
void JunctionsExtractor::save_targets(bam_hdr_t *header) {
    for (int i = 0; i < header->n_targets; i++) {
        targets_.insert(targets_.end(), string(header->target_name[i]));
    }
}

// The workhorse - identifies junctions from BAM
void JunctionsExtractor::identify_junctions_from_bam() {
    samFile *in = sam_open(bam_.c_str(), "r");
    if (in == NULL) {
        throw runtime_error("Unable to open BAM/SAM/CRAM file: " + bam_);
    }
    bam_hdr_t *header = sam_hdr_read(in);
    save_targets(header);

    bam1_t *aln = bam_init1();
    while(sam_read1(in, header, aln) >= 0) {
        try {
            parse_alignment_into_junctions(header, aln);
        } catch (const std::logic_error& e) {
            cerr << "Warning: error processing read: " << e.what() << endl;
        }
    }
    bam_destroy1(aln);
    bam_hdr_destroy(header);
    sam_close(in);
}

// Create the junctions vector from the map
void JunctionsExtractor::create_junctions_vector() {
#if 0 //FIXME
    for (map<string, Junction*> :: iterator it = junctions_.begin();
        it != junctions_.end(); it++) {
        Junction j1 = it->second;
        junctions_vector_.push_back(j1);
    }
#endif
}