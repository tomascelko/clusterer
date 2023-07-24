//a package which includes basic headers reuqired for managing the dataflow
//and implementing a node
#include "data_buffer.h"
#include "pipe_writer.h"
#include "pipe_reader.h"
#include "i_data_producer.h"
#include "i_data_consumer.h"
#include "pipe.h"
#include "pipe_descriptor.h"
#include "i_controlable_source.h"
#include "multi_cluster_pipe_reader.h"
#include "i_simple_consumer.h"
#include "i_simple_producer.h"
#include "multi_pipe_writer.h"
#include "i_multi_producer.h"
#include "dataflow_controller.h"
#pragma once