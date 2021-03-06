/*
* Roboception GmbH
* Munich, Germany
* www.roboception.com
*
* Copyright (c) 2017 Roboception GmbH
* All rights reserved
*
* Author: Raphael Schaller
*/

#ifndef REGISTER_RESOURCES
#define REGISTER_RESOURCES

/**
 * @brief Registers virtual resource files using wxMemoryFSHandler.
 * They can be retrieved in wxWidgets with the "memory:" prefix.
 */
void registerResources();

#endif /* REGISTER_RESOURCES */
