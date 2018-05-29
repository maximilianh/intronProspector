NAME
====

**intronProspector** — Extract putative junctions from RNA-Seq alignments

SYNOPSIS
========

`intronProspector [options] [readaligns]`

DESCRIPTION
===========

Find putative intron junctions in a RNA-Seq alignment. The *readaligns* file maybe in SAM, BAM, or CRAM format and does not need to be sorted or indexed and maybe streamed.

This program allows for calling splice junctions indebtedness of the alignment program.  It maybe used in a pipeline, copying the alignment file on `stdin` to `stdout`.  It can sit between an aligner outputting SAM and `samtools` to convert to BAM/CRAM.

Options
-------

`-h, --help`

> Prints brief usage information.

`-v, --version`

> Prints the current version number.

`-a INT, --min-anchor-length=INT`

> Minimum anchor length. Junctions which satisfy a minimum anchor length on both sides are reported.  Mismatch bases don't count towards the meeting this threshold.  The default is 8 bases.

`-i INT, --min-intron_length=INT`

> Minimum intron length. The default is 70 bases.

`-I INT,  --max-intron_length=INT`

> Maximum intron length. The default is 500000 bases.

`-s STRING, --strandness=STRING`

> Strand specificity of RNA library preparation.  Use `UN` for unstranded, `RF` for first-strand, `FR` for second-strand (case-insensitive).  The default is `UN`.  This is used to set the strand in the junction-format BED file.

`-j FILE, --junction-bed=FILE`

> Write the junction calls and support anchors to this file.  This is in the same format as ToHat `junctions.bed` and `regtools junction extract` output.  It UCSC BED track, with each junction consists of two connected BED blocks, where each block is as long as the maximal overhang of any read spanning the junction. The score is the number of alignments spanning the junction.

`-n FILE, --intron-bed=FILE`

> Write the intron BED with the bounds of the intron. The score is the number of alignments spanning the junction.


BUGS
====

See GitHub Issues: <https://github.com/diekhans/intronProspector/issues>

AUTHOR
======

Mark Diekhans <markd@ucsc.edu>

Base on code from RegTools <https://github.com/griffithlab/regtools>
by Avinash Ramu <aramu@genome.wustl.edu>.
