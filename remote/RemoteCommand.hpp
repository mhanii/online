#pragma once

#include <string>
#include <memory>

/**
 * RemoteCommand - Base class for remote commands
 *
 * This class defines the interface for remote commands that can be
 * executed by the RController.
 */
class RemoteCommand
{
public:
    RemoteCommand() = default;
    virtual ~RemoteCommand() = default;

    /**
     * Execute the command
     *
     * @return true if the command was executed successfully
     */
    virtual bool execute() = 0;

    /**
     * Get the command name
     *
     * @return the command name
     */
    virtual std::string getName() const = 0;
};
