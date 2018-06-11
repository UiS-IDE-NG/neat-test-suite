#!/usr/bin/env Rscript

library(gtools)

args <- commandArgs(trailingOnly = TRUE)

tables_dir <- args[1]
filename <- args[2]
output_dir <- args[3]
output_filename <- args[4]

setwd(tables_dir)

datatable <- read.table(filename, header = TRUE, fill = TRUE)
datatable <- datatable[ , mixedorder(names(datatable))]

datatable_max <- apply(datatable, 2, max, na.rm=TRUE)
datatable_min <- apply(datatable, 2, min, na.rm=TRUE)
datatable_median <- apply(datatable, 2, median, na.rm=TRUE)
datatable_percentiles <- apply(datatable, 2, quantile, c(0.10, 0.90), na.rm=TRUE)

setwd(output_dir)
sink(output_filename)
for (i in 1:ncol(datatable)) {
    val_min <- getElement(datatable_min, i)
    val_10 <- getElement(datatable_percentiles, i*2 - 1)
    val_median <- getElement(datatable_median, i)
    val_90 <- getElement(datatable_percentiles, i*2)
    val_max <- getElement(datatable_max, i)
    cat(i)
    cat(" ")
    cat(val_min)
    cat(" ")
    cat(val_10)
    cat(" ")
    cat(val_median)
    cat(" ")
    cat(val_90)
    cat(" ")
    cat(val_max)
    cat(" ")
    cat(substring(colnames(datatable)[i], 2))
    cat("\n")
}
sink()