#!/bin/bash

#NOTE: script to run on the incaview-hub which deploys instances of the core-template

# parameters:
# $1 : instance name
# $2 : username


# Create a disk from the snapshot-inca, which contains all the exe files we want.
gcloud compute disks create $1-disk --size 10 --zone europe-west3-a --source-snapshot snapshot-inca --type pd-standard

# Create a compute instance from the core-template, with this disk attached
gcloud compute instances create --source-instance-template core-template --zone europe-west3-a --disk=name=$1-disk,device-name=$1-disk,mode=rw,boot=yes,auto-delete=yes

# Delete previous ssh-rsa keys in case they exist
rm -f keys/$2
rm -f keys/$2.pub

# Generate new ssh-rsa keys
ssh-keygen -t rsa -f keys/$2 -C $2 -N '' -q

# Create a key list containing this key to update the keys for the instance
echo $2: | cat -keys/$2.pub | tr '\n' ' ' > keys/$2-list

# Add the ssh-rsa keys to the instance
gcloud compute instances add-metadata $1 --zone europe-west3-a --metadata-from-file ssh-keys=keys/$2-list

# Delete the key list.
rm keys/$2-list