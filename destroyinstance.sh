#!/bin/bash

#NOTE: script to run on the incaview-hub which destroys a compute instance

# parameters:
# $1 : instance name

gcloud compute instances delete $1 --zone=europe-west3-a -q
