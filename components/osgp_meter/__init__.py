import esphome.codegen as cg
from esphome.components import uart

osgp_ns = cg.esphome_ns.namespace("osgp_meter")
OSGPMeter = osgp_ns.class_("OSGPMeter", cg.PollingComponent, uart.UARTDevice)
