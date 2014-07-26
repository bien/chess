
args <- commandArgs(trailingOnly=TRUE)
traindata <- read.table(args[1])
fit <- lm(V1 ~ ., data=traindata)
summary(fit)