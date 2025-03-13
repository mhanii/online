#!/bin/bash

# Script to help migrate from the old ModelApiClient structure to the new modular structure

echo "Starting migration to new modular structure..."

# Backup the old files
echo "Backing up old files..."
mkdir -p backup
cp ModelApiClient.cpp backup/
cp ModelApiClient.hpp backup/

# Remove the old object files to force recompilation
echo "Removing old object files..."
rm -f *.o

# Rebuild the project
echo "Rebuilding the project..."
cd ..
make clean
make

echo "Migration complete!"
echo "If you encounter any issues, the old files are backed up in the 'backup' directory."
echo "You may need to manually update any code that directly references ModelApiClient."
echo ""
echo "Note: The new implementation uses a simplified model handling approach."
echo "The 'model' prefix in commands is just used to route the request to the LLM pipeline,"
echo "not to select a specific model. The default provider is used for all requests."