#!/usr/bin/env Rscript

library(gtools)

args <- commandArgs(trailingOnly = TRUE)

tables_dir <- args[1]
filename <- args[2]
output_dir <- args[3]
output_filename <- args[4]

flows <- c(1, 2, 4, 8, 16, 32, 64, 128, 256)

setwd(tables_dir)

datatable <- read.table(filename, header = FALSE, fill = TRUE)
datatable <- datatable[ , mixedorder(names(datatable))]

datatable_max <- apply(datatable, 1, max, na.rm=TRUE)
datatable_min <- apply(datatable, 1, min, na.rm=TRUE)
datatable_median <- apply(datatable, 1, median, na.rm=TRUE)
datatable_percentiles <- apply(datatable, 1, quantile, c(0.10, 0.90), na.rm=TRUE)

setwd(output_dir)
sink(output_filename)
for (i in 1:nrow(datatable)) {
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
    cat(flows[i])
    cat("\n")
}
sink()
