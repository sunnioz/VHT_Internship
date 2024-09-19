-- Create a new protocol named NgAP_Paging
local ngap_paging_proto = Proto("NgAP_Paging", "NgAP Paging Message")

-- Define the fields for the NgAP_Paging protocol
local fields = ngap_paging_proto.fields
fields.message_type = ProtoField.int32("ngap_paging.message_type", "Message Type", base.DEC)
fields.ng_5g_s_tmsi = ProtoField.int32("ngap_paging.ng_5g_s_tmsi", "NG 5G-S-TMSI", base.DEC)
fields.tai = ProtoField.int32("ngap_paging.tai", "TAI", base.DEC)
fields.cn_domain = ProtoField.int32("ngap_paging.cn_domain", "CN Domain", base.DEC)

-- Dissector function
function ngap_paging_proto.dissector(buffer, pinfo, tree)
    -- Set the protocol column in the UI
    pinfo.cols.protocol = "NgAP Paging"

    -- Create the protocol tree
    local subtree = tree:add(ngap_paging_proto, buffer(), "NgAP Paging Message")

    -- Check if the buffer is large enough to contain the entire message
    if buffer:len() < 16 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "NgAP Paging message length is too short")
        return
    end

    -- Add fields to the tree
    subtree:add_le(fields.message_type, buffer(0, 4))
    subtree:add_le(fields.ng_5g_s_tmsi, buffer(4, 4))
    subtree:add_le(fields.tai, buffer(8, 4))
    subtree:add_le(fields.cn_domain, buffer(12, 4))
end

-- Register the dissector to TCP port 6000
local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(6000, ngap_paging_proto)
