const std = @import("std");
const io = std.io;
const mem = std.mem;
const fmt = std.fmt;
const heap = std.heap;
const ArrayList = std.ArrayList;

const USAGE = "USAGE: vcd < in.vcd > out.ascii :\n";
const PROLOG = "Fatal error. Send the VCD on https://github.com/yne/vcd/issues";

const SFL = 127; // scanf Token limit
const MAX_SCOPE = 32; // how many char scopes[] to allocate
const MAX_CHANNEL = 400; // how many Channel to allocate
const ITV_TIME = 10; // sample interval to display timestamp
const VALUES = "0123456789zZxXbU-"; // allowed bus values/types

const Token = [SFL + 1]u8;

const PrintOpt = struct {
    low: []const u8,
    raise: []const u8,
    high: []const u8,
    drown: []const u8,
    start: []const u8,
    end: []const u8,
    skip: u32,
};

const Sample = struct {
    // could have as much state as it bus size but nobody handle such case
    // 'UZX' => show state
    // '\0' => show .val
    state: u8,
    val: u32,
};

const Channel = struct {
    size: u32,
    scope: u32,
    name: Token,
    samples: ArrayList(Sample),

    fn init(allocator: mem.Allocator) Channel {
        return .{
            .size = 0,
            .scope = 0,
            .name = mem.zeroes(Token),
            .samples = ArrayList(Sample).init(allocator),
        };
    }

    fn deinit(self: *Channel) void {
        self.samples.deinit();
    }
};

const ParseCtx = struct {
    allocator: mem.Allocator,
    ch: [MAX_CHANNEL]Channel,
    scopes: [MAX_SCOPE]Token,
    total: u32,
    scope_count: u32,
    chan_str: bool,
    scale: f32,
    date: Token,
    version: Token,
    unit: Token,
    scope_cur: u32,
    scope_lim: u32,
    ch_lim: u32,
    sz_lim: u32,

    fn init(allocator: mem.Allocator) ParseCtx {
        var ctx = ParseCtx{
            .allocator = allocator,
            .ch = undefined,
            .scopes = undefined,
            .total = 0,
            .scope_count = 0,
            .chan_str = false,
            .scale = 0.0,
            .date = mem.zeroes(Token),
            .version = mem.zeroes(Token),
            .unit = mem.zeroes(Token),
            .scope_cur = 0,
            .scope_lim = 0,
            .ch_lim = 0,
            .sz_lim = 0,
        };
        for (&ctx.ch) |*c| {
            c.* = Channel.init(allocator);
        }
        for (&ctx.scopes) |*s| {
            s.* = mem.zeroes(Token);
        }
        return ctx;
    }

    fn deinit(self: *ParseCtx) void {
        for (&self.ch) |*c| {
            c.deinit();
        }
    }
};

// Convert a base-94 or 'c'+num chan id (!...~) to integer
fn chanId(str_id: []const u8, isStr: bool) !usize {
    var id: usize = 0;
    if (isStr) {
        id = try fmt.parseInt(usize, str_id[1..], 10);
    } else {
        var i = str_id.len;
        while (i > 0) {
            i -= 1;
            id = (id * 94) + @as(usize, str_id[i] - '!');
        }
    }
    if (id >= MAX_CHANNEL) {
        std.debug.print("MAX_CHANNEL reached ({d}), rebuild with different MAX_CHANNEL\n", .{id});
        return error.MaxChannelReached;
    }
    return id;
}

fn unilen(s: []const u8) usize {
    var j: usize = 0;
    for (s) |byte| {
        if ((byte & 0xc0) != 0x80) j += 1;
    }
    return j;
}

fn tokenize(line: []const u8, allocator: mem.Allocator) !ArrayList([]const u8) {
    var tokens = ArrayList([]const u8).init(allocator);
    var it = mem.tokenizeAny(u8, line, " \t\n\r$");
    while (it.next()) |token| {
        try tokens.append(token);
    }
    return tokens;
}

fn parseVcdInstruction(p: *ParseCtx, reader: anytype, token: []const u8) !void {
    if (mem.eql(u8, token, "var")) {
        // Read rest of the line for var instruction
        var buf: [1024]u8 = undefined;
        const line = (try reader.readUntilDelimiterOrEof(&buf, '\n')) orelse return;
        
        // Parse: type size id name
        var it = mem.tokenizeAny(u8, line, " \t\n");
        const type_str = it.next() orelse return;
        _ = type_str; // unused
        const size_str = it.next() orelse return;
        const id_str = it.next() orelse return;
        const name_str = it.rest();
        
        const size = try fmt.parseInt(u32, size_str, 10);
        const id = try chanId(id_str, p.chan_str);
        
        p.ch[id].size = size;
        p.ch[id].scope = p.scope_cur;
        @memset(&p.ch[id].name, 0);
        const name_len = @min(name_str.len, SFL);
        @memcpy(p.ch[id].name[0..name_len], name_str[0..name_len]);
        
        p.ch_lim = @max(p.ch_lim, @as(u32, @intCast(name_len)));
        p.sz_lim = @max(p.sz_lim, size);
    } else if (mem.eql(u8, token, "scope")) {
        p.scope_count += 1;
        if (p.scope_count >= MAX_SCOPE) {
            return error.MaxScopeReached;
        }
        p.scope_cur = p.scope_count;
        
        var buf: [1024]u8 = undefined;
        const line = (try reader.readUntilDelimiterOrEof(&buf, '\n')) orelse return;
        var it = mem.tokenizeAny(u8, line, " \t\n");
        const _scope_type = it.next() orelse return;
        _ = _scope_type; // unused
        const scope_name = it.next() orelse return;
        
        @memset(&p.scopes[p.scope_cur], 0);
        const len = @min(scope_name.len, SFL);
        @memcpy(p.scopes[p.scope_cur][0..len], scope_name[0..len]);
        p.scope_lim = @max(p.scope_lim, @as(u32, @intCast(len)));
    } else if (mem.eql(u8, token, "date")) {
        var buf: [1024]u8 = undefined;
        const line = (try reader.readUntilDelimiterOrEof(&buf, '\n')) orelse return;
        @memset(&p.date, 0);
        const len = @min(mem.trim(u8, line, " \t\n").len, SFL);
        @memcpy(p.date[0..len], mem.trim(u8, line, " \t\n")[0..len]);
    } else if (mem.eql(u8, token, "version")) {
        var buf: [1024]u8 = undefined;
        const line = (try reader.readUntilDelimiterOrEof(&buf, '\n')) orelse return;
        @memset(&p.version, 0);
        const len = @min(mem.trim(u8, line, " \t\n").len, SFL);
        @memcpy(p.version[0..len], mem.trim(u8, line, " \t\n")[0..len]);
        // ROHD use 's'+digit channel ID sequencing
        p.chan_str = mem.indexOf(u8, &p.version, "ROHD") != null;
    } else if (mem.eql(u8, token, "timescale")) {
        var buf: [1024]u8 = undefined;
        const line = (try reader.readUntilDelimiterOrEof(&buf, '\n')) orelse return;
        var it = mem.tokenizeAny(u8, line, " \t\n");
        const scale_str = it.next() orelse return;
        const unit_str = it.rest();
        
        p.scale = try fmt.parseFloat(f32, scale_str);
        @memset(&p.unit, 0);
        const len = @min(unit_str.len, SFL);
        @memcpy(p.unit[0..len], unit_str[0..len]);
    } else if (mem.eql(u8, token, "comment") or 
               mem.eql(u8, token, "upscope") or
               mem.eql(u8, token, "enddefinitions") or
               mem.eql(u8, token, "dumpvars") or
               mem.eql(u8, token, "end")) {
        // Read until newline
        var buf: [1024]u8 = undefined;
        _ = try reader.readUntilDelimiterOrEof(&buf, '\n');
        if (mem.eql(u8, token, "upscope")) {
            p.scope_cur = 0; // back to the root
        }
    }
}

fn parseVcdTimestamp(p: *ParseCtx, reader: anytype) !void {
    // copy previous sample on every channel
    if (p.total > 0) {
        for (&p.ch) |*ch| {
            if (ch.size == 0) continue; // skip unused channels
            const prev = ch.samples.items[p.total - 1];
            try ch.samples.append(prev);
        }
    } else {
        for (&p.ch) |*ch| {
            if (ch.size == 0) continue;
            try ch.samples.append(.{ .state = 0, .val = 0 });
        }
    }
    
    // Read the timestamp value
    var buf: [256]u8 = undefined;
    var i: usize = 0;
    while (try reader.readByte()) |byte| {
        if (byte == '\n' or byte == ' ' or byte == '\r' or byte == '\t') break;
        if (i < buf.len) {
            buf[i] = byte;
            i += 1;
        }
    }
    
    p.total += 1;
}

fn parseVcdSample(p: *ParseCtx, reader: anytype, c: u8) !void {
    var s = Sample{ .state = 0, .val = 0 };
    
    if (c == 'b') {
        while (try reader.readByte()) |byte| {
            if (byte == ' ' or byte == '\n') break;
            if (byte == '0' or byte == '1') {
                s.val = s.val * 2 + (byte - '0');
            } else if (mem.indexOfScalar(u8, VALUES, byte) != null) {
                s.state = byte;
            } else {
                std.debug.print("Unknown sample value: {c}\n", .{byte});
                return error.UnknownSampleValue;
            }
        }
    } else {
        s.state = if (std.ascii.isAlphabetic(c)) c else 0;
        s.val = if (std.ascii.isDigit(c)) c - '0' else 0;
    }
    
    // Read the channel ID
    var id_buf: [256]u8 = undefined;
    var i: usize = 0;
    while (try reader.readByte()) |byte| {
        if (byte == ' ' or byte == '\n' or byte == '\r' or byte == '\t') break;
        if (i < id_buf.len) {
            id_buf[i] = byte;
            i += 1;
        }
    }
    
    if (p.total == 0) return; // ROHD define value BEFORE timestamp #0
    const id = try chanId(id_buf[0..i], p.chan_str);
    if (p.ch[id].samples.items.len > 0) {
        p.ch[id].samples.items[p.total - 1] = s;
    }
}

fn parseVcd(p: *ParseCtx, reader: anytype) !void {
    var in_instruction = false;
    var buf: [256]u8 = undefined;
    var token_buf: [256]u8 = undefined;
    var token_len: usize = 0;
    
    while (true) {
        const byte = reader.readByte() catch |err| {
            if (err == error.EndOfStream) break;
            return err;
        };
        
        if (std.ascii.isWhitespace(byte)) continue;
        
        if (byte == '$') {
            // Read the instruction token
            token_len = 0;
            while (try reader.readByte()) |b| {
                if (std.ascii.isWhitespace(b)) break;
                if (token_len < token_buf.len) {
                    token_buf[token_len] = b;
                    token_len += 1;
                }
            }
            try parseVcdInstruction(p, reader, token_buf[0..token_len]);
        } else if (byte == '#') {
            try parseVcdTimestamp(p, reader);
        } else if (mem.indexOfScalar(u8, VALUES, byte) != null) {
            try parseVcdSample(p, reader, byte);
        }
    }
}

fn printYml(p: *ParseCtx, opt: *const PrintOpt, writer: anytype) !void {
    if (unilen(opt.high) != 1) {
        std.debug.print("high waveform length must be 1\n", .{});
        return error.InvalidWaveform;
    }
    if (unilen(opt.low) != 1) {
        std.debug.print("low waveform length must be 1\n", .{});
        return error.InvalidWaveform;
    }
    if (unilen(opt.drown) > 1) {
        std.debug.print("drown waveform length must be 1 or empty\n", .{});
        return error.InvalidWaveform;
    }
    if (unilen(opt.raise) > 1) {
        std.debug.print("raise waveform length must be 1 or empty\n", .{});
        return error.InvalidWaveform;
    }
    
    const zoom: i32 = @intCast((p.sz_lim + 7) >> 2); // how many char per sample (8bit => 2)
    const trans = opt.drown.len > 0 and opt.raise.len > 0;
    
    try writer.print("global:\n", .{});
    try writer.print("  zoom: {d}\n", .{zoom});
    try writer.print("  date: {s}\n", .{mem.sliceTo(&p.date, 0)});
    try writer.print("  total: {d}\n", .{p.total});
    try writer.print("  skip: {d}\n", .{opt.skip});
    try writer.print("  time:\n", .{});
    try writer.print("    scale: {d:.2}\n", .{p.scale});
    const unit_str = mem.sliceTo(&p.unit, 0);
    try writer.print("    unit: {s}\n", .{if (unit_str.len > 0) unit_str else "?"});
    try writer.print("    {s: <[1]}: {s}", .{ "line", p.ch_lim, opt.start });
    
    var smpl: f64 = @floatFromInt(opt.skip);
    while (smpl < @as(f64, @floatFromInt(p.total))) : (smpl += ITV_TIME) {
        try writer.print("{d: <[1]}", .{ smpl * p.scale, ITV_TIME * zoom - 1 });
    }
    try writer.print("{s}\nchannels:\n", .{opt.end});
    
    var prev_scope: ?u32 = null;
    for (&p.ch, 0..) |*ch, idx| {
        if (ch.size == 0) continue;
        
        // print scope (if changed)
        if (prev_scope == null or prev_scope.? != ch.scope) {
            const scope_name = mem.sliceTo(&p.scopes[ch.scope], 0);
            const name = if (scope_name.len > 0) scope_name else "default";
            try writer.print("  {s}:\n", .{name});
            prev_scope = ch.scope;
        }
        
        const ch_name = mem.sliceTo(&ch.name, 0);
        try writer.print("    {s: <[1]}: {s}", .{ ch_name, p.ch_lim, opt.start });
        
        var sample_idx = opt.skip;
        while (sample_idx < p.total) : (sample_idx += 1) {
            if (sample_idx >= ch.samples.items.len) break;
            const s = ch.samples.items[sample_idx];
            const prev = if (sample_idx > 0 and sample_idx - 1 < ch.samples.items.len)
                ch.samples.items[sample_idx - 1]
            else
                s;
            
            if (s.state != 0) { // state data: UUUUZZZZ-
                try writer.print("{c: <[1]}", .{ s.state, zoom });
            } else if (ch.size == 1) { // binary wave: ▁▁/▔▔
                var w: u32 = 0;
                while (w < zoom) : (w += 1) {
                    if (w == 0 and trans and s.state == prev.state and s.val != prev.val) {
                        try writer.print("{s}", .{if (prev.val != 0) opt.drown else opt.raise});
                    } else {
                        try writer.print("{s}", .{if (s.val != 0) opt.high else opt.low});
                    }
                }
            } else { // bus : show hex value
                try writer.print("{X: <[1]}", .{ s.val, zoom });
            }
        }
        try writer.print("{s}\n", .{opt.end});
    }
}

pub fn main() !void {
    var gpa = heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();
    
    const stdin = io.getStdIn().reader();
    const stdout = io.getStdOut().writer();
    
    var opt = PrintOpt{
        .low = std.posix.getenv("LOW") orelse "▁",
        .raise = std.posix.getenv("RAISE") orelse "╱",
        .high = std.posix.getenv("HIGH") orelse "▔",
        .drown = std.posix.getenv("DROWN") orelse "╲",
        .start = std.posix.getenv("STX") orelse "\"",
        .end = std.posix.getenv("ETX") orelse "\"",
        .skip = blk: {
            if (std.posix.getenv("SKIP")) |skip_str| {
                break :blk try fmt.parseInt(u32, skip_str, 10);
            }
            break :blk 0;
        },
    };
    
    var ctx = ParseCtx.init(allocator);
    defer ctx.deinit();
    
    try parseVcd(&ctx, stdin);
    try printYml(&ctx, &opt, stdout);
}
