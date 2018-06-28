use mir;
use std::collections::{HashMap, VecDeque};

// groups extracted nodes into subsurfaces
pub fn group_extracted(
    surface: &mut mir::Surface,
    allocator: &mut mir::IdAllocator,
) -> Vec<mir::Surface> {
    GroupExtractor::new(surface).extract_groups(allocator)
}

type ValueGroupRef = usize;
type NodeRef = usize;
type ValueSocketRef = (NodeRef, usize);

#[derive(Debug)]
struct ExtractGroup {
    pub sources: Vec<ValueGroupRef>,
    pub destinations: Vec<ValueGroupRef>,
    pub nodes: Vec<NodeRef>,
    pub value_groups: Vec<ValueGroupRef>,
}

#[derive(Debug, Clone)]
struct ValueGroupData {
    pub sockets: Vec<ValueSocketRef>,
}

struct GroupExtractor<'a> {
    surface: &'a mut mir::Surface,
}

impl ExtractGroup {
    pub fn new() -> Self {
        ExtractGroup {
            sources: Vec::new(),
            destinations: Vec::new(),
            nodes: Vec::new(),
            value_groups: Vec::new(),
        }
    }
}

impl ValueGroupData {
    pub fn new() -> Self {
        ValueGroupData {
            sockets: Vec::new(),
        }
    }
}

impl<'a> GroupExtractor<'a> {
    pub fn new(surface: &'a mut mir::Surface) -> Self {
        GroupExtractor { surface }
    }

    pub fn extract_groups(&mut self, allocator: &mut mir::IdAllocator) -> Vec<mir::Surface> {
        self.find_extracted_groups()
            .into_iter()
            .enumerate()
            .map(|(index, group)| self.extract_group(allocator, index, group))
            .collect()
    }

    fn extract_group(
        &mut self,
        allocator: &mut mir::IdAllocator,
        extract_index: usize,
        extract_group: ExtractGroup,
    ) -> mir::Surface {
        // Here we move the extracted nodes into a new ExtractGroup node.
        // We attempt to keep the surface structure as intact as possible: it's inevitable that
        // we have to move nodes, but we avoid removing groups here, instead making sockets in the
        // new node to "forward" the group out - the groups we need to do this to are those listed
        // in the extract group provided.
        // The combination of "remove_dead_sockets" and "remove_dead_groups" passes will remove any
        // sockets that aren't necessary.

        let mut new_value_groups = Vec::new();
        let mut new_sockets = Vec::new();
        let mut new_value_group_indexes = HashMap::new();
        let mut new_nodes = Vec::new();

        for &parent_group_index in &extract_group.value_groups {
            let new_group_index = new_value_groups.len();
            let parent_group = &self.surface.groups[parent_group_index];

            // Note: we expect the parent value group to have an array type here instead of
            // 'upgrading' the group's type here or later on. This ensures we don't need to do
            // anything messy like update sockets and value groups up the tree.
            // Inside the extracted surface though, the group does not have an array type.
            new_value_groups.push(mir::ValueGroup::new(
                parent_group
                    .value_type
                    .base_type()
                    .unwrap_or(&parent_group.value_type)
                    .clone(),
                mir::ValueGroupSource::Socket(new_group_index),
            ));

            new_sockets.push(mir::ValueSocket::new(
                parent_group_index,
                false,
                false,
                false,
            ));

            new_value_group_indexes.insert(parent_group_index, new_group_index);
        }

        for (removed_count, &node_index) in extract_group.nodes.iter().enumerate() {
            let real_node_index = node_index - removed_count;
            let mut new_node = self.surface.nodes.remove(real_node_index);

            // update socket indexes to the indexes in the new group
            for socket in new_node.sockets.iter_mut() {
                let remapped_id = new_value_group_indexes[&socket.group_id];
                socket.group_id = remapped_id;

                // update the sockets `value_written` and `value_read` flags
                if socket.value_written {
                    new_sockets[remapped_id].value_written = true;
                }
                if socket.value_read {
                    new_sockets[remapped_id].value_read = true;
                }
            }

            new_nodes.push(new_node)
        }

        let new_surface = mir::Surface::new(
            mir::SurfaceId::new(
                format!("{}.extracted{}", self.surface.id.debug_name, extract_index),
                allocator,
            ),
            new_value_groups,
            new_nodes,
        );
        let new_node = mir::Node::new(
            new_sockets,
            mir::NodeData::ExtractGroup {
                surface: new_surface.id.clone(),
                source_sockets: extract_group
                    .sources
                    .into_iter()
                    .map(|group| new_value_group_indexes[&group])
                    .collect(),
                dest_sockets: extract_group
                    .destinations
                    .into_iter()
                    .map(|group| new_value_group_indexes[&group])
                    .collect(),
            },
        );

        // add the new node to the parent surface
        self.surface.nodes.push(new_node);

        new_surface
    }

    fn find_extracted_groups(&self) -> Vec<ExtractGroup> {
        let mut extract_groups = Vec::new();
        let mut value_groups = vec![ValueGroupData::new(); self.surface.groups.len()];
        let mut trace_queue = VecDeque::new();
        let mut value_group_extracts = HashMap::new();
        let mut node_extracts = HashMap::new();

        // In order to find extracted groups, we need two things:
        //  - The sockets that are 'connected' to a group, and whether that group then contains an extractor
        //  - The extract sockets, which seed our breadth first search. Extract sockets that are
        //    written to also become sources to their extract group, ones that are read from become
        //    destinations.
        // For efficiency, we can do these both in the same loop!
        for (node_index, node) in self.surface.nodes.iter().enumerate() {
            for (socket_index, socket) in node.sockets.iter().enumerate() {
                let socket_ref = (node_index, socket_index);
                let value_group = &mut value_groups[socket.group_id];
                value_group.sockets.push(socket_ref);

                if socket.is_extractor {
                    let extract_group_index =
                        if let Some(&existing_group) = value_group_extracts.get(&socket.group_id) {
                            existing_group
                        } else {
                            let group_index = extract_groups.len();
                            value_group_extracts.insert(socket.group_id, group_index);
                            extract_groups.push(ExtractGroup::new());
                            group_index
                        };
                    trace_queue.push_back(socket.group_id);

                    if socket.value_written {
                        extract_groups[extract_group_index]
                            .sources
                            .push(socket.group_id);
                    }
                    if socket.value_read {
                        extract_groups[extract_group_index]
                            .destinations
                            .push(socket.group_id);
                    }
                }
            }
        }

        // Now we can breadth-first search through groups.
        // This basically means looking at each node attached to a group, and either:
        //  - Spreading to any value groups from other sockets on the node _if it doesn't already have an extract group_
        //  - Merging extract groups if it does
        // We only propagate into nodes where their socket _reads_ from a group, and never propagate into extract sockets
        while let Some(value_group_index) = trace_queue.pop_front() {
            let extract_group_index = value_group_extracts[&value_group_index];

            extract_groups[extract_group_index]
                .value_groups
                .push(value_group_index);

            // look at each of the sockets connected to this value group
            for &(connected_node_index, connected_socket_index) in
                &value_groups[value_group_index].sockets
            {
                let connected_node = &self.surface.nodes[connected_node_index];
                let connected_socket = &connected_node.sockets[connected_socket_index];

                // if the node already has an extractor group, merge it with ours and stop there
                // otherwise, it becomes part of our group and we propagate to all of its value groups
                if let Some(connected_extract_group_index) =
                    node_extracts.get(&connected_node_index).cloned()
                {
                    GroupExtractor::merge_extract_groups(
                        extract_group_index,
                        connected_extract_group_index,
                        &mut extract_groups,
                        &mut value_group_extracts,
                        &mut node_extracts,
                    )
                } else if !connected_socket.is_extractor && connected_socket.value_read {
                    extract_groups[extract_group_index]
                        .nodes
                        .push(connected_node_index);
                    node_extracts.insert(connected_node_index, extract_group_index);

                    for socket in &connected_node.sockets {
                        if value_group_extracts.get(&socket.group_id).is_none() {
                            value_group_extracts.insert(socket.group_id, extract_group_index);
                            trace_queue.push_back(socket.group_id);
                        }
                    }
                }
            }
        }

        // remove any empty extract groups (with no nodes)
        extract_groups.retain(|group| !group.nodes.is_empty());
        extract_groups
    }

    fn merge_extract_groups(
        dest_index: usize,
        src_index: usize,
        groups: &mut [ExtractGroup],
        value_group_extracts: &mut HashMap<ValueGroupRef, usize>,
        node_extracts: &mut HashMap<NodeRef, usize>,
    ) {
        // don't try and merge if the groups are the same
        if dest_index == src_index {
            return;
        }

        {
            // migrate all references over to the destination group in the two HashMaps
            let src_group = &groups[src_index];
            for &source_ref in &src_group.sources {
                value_group_extracts.insert(source_ref, dest_index);
            }
            for &dest_ref in &src_group.destinations {
                value_group_extracts.insert(dest_ref, dest_index);
            }
            for &node_ref in &src_group.nodes {
                node_extracts.insert(node_ref, dest_index);
            }
        }

        {
            // concatenate source/node/group values onto destination
            let sources = groups[src_index].sources.to_vec();
            let destinations = groups[src_index].destinations.to_vec();
            let nodes = groups[src_index].nodes.to_vec();
            let value_groups = groups[src_index].value_groups.to_vec();

            let dest_group = &mut groups[dest_index];
            dest_group.sources.extend(sources);
            dest_group.destinations.extend(destinations);
            dest_group.nodes.extend(nodes);
            dest_group.value_groups.extend(value_groups);
        }

        {
            // remove all items from the source
            let src_group = &mut groups[src_index];
            src_group.sources.clear();
            src_group.destinations.clear();
            src_group.nodes.clear();
            src_group.value_groups.clear();
        }
    }
}
