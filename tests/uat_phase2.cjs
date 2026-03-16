// Phase 2 UAT: Automated Scene CRUD testing via TCP
const net = require('net');

const PORT = 6800;
const HOST = '127.0.0.1';
let msgId = 0;
let client;
let buffer = '';
let responseResolve = null;

function nextId() { return ++msgId; }

function send(obj) {
  return new Promise((resolve, reject) => {
    responseResolve = resolve;
    const data = JSON.stringify(obj) + '\n';
    client.write(data);
    // Timeout after 5s
    setTimeout(() => {
      if (responseResolve === resolve) {
        responseResolve = null;
        reject(new Error('Timeout waiting for response'));
      }
    }, 5000);
  });
}

function sendNotification(obj) {
  client.write(JSON.stringify(obj) + '\n');
  return new Promise(r => setTimeout(r, 300));
}

function callTool(name, args) {
  const id = nextId();
  return send({
    jsonrpc: '2.0',
    id,
    method: 'tools/call',
    params: { name, arguments: args }
  });
}

function getToolResult(resp) {
  try {
    if (resp.result && resp.result.content && resp.result.content[0]) {
      return JSON.parse(resp.result.content[0].text);
    }
  } catch (e) {}
  return resp;
}

const results = [];
function pass(name, detail) {
  results.push({ name, status: 'pass', detail });
  console.log(`  PASS: ${name}${detail ? ' -- ' + detail : ''}`);
}
function fail(name, detail) {
  results.push({ name, status: 'FAIL', detail });
  console.log(`  FAIL: ${name} -- ${detail}`);
}
function skip(name, detail) {
  results.push({ name, status: 'skip', detail });
  console.log(`  SKIP: ${name} -- ${detail}`);
}

async function runTests() {
  console.log('\n=== Phase 2 UAT: Scene CRUD Tools ===\n');

  // --- Test 1: Initialize + tools/list ---
  console.log('[Test 1] tools/list shows 4 tools');
  try {
    const initResp = await send({
      jsonrpc: '2.0', id: nextId(), method: 'initialize',
      params: { protocolVersion: '2025-03-26', capabilities: {}, clientInfo: { name: 'uat-test', version: '1.0' } }
    });
    await sendNotification({ jsonrpc: '2.0', method: 'notifications/initialized' });

    const listResp = await send({ jsonrpc: '2.0', id: nextId(), method: 'tools/list' });
    const tools = listResp.result.tools.map(t => t.name).sort();
    const expected = ['create_node', 'delete_node', 'get_scene_tree', 'set_node_property'];
    if (JSON.stringify(tools) === JSON.stringify(expected)) {
      pass('tools/list shows 4 tools', tools.join(', '));
    } else {
      fail('tools/list shows 4 tools', `Got: ${tools.join(', ')}`);
    }
  } catch (e) {
    fail('tools/list shows 4 tools', e.message);
  }

  // --- Cleanup: delete any leftover test nodes ---
  console.log('\n[Cleanup] Removing leftover test nodes...');
  for (const path of ['TestSprite/Child', 'ColorSprite', 'TestSprite']) {
    try { await callTool('delete_node', { node_path: path }); } catch (e) {}
  }
  await new Promise(r => setTimeout(r, 300));

  // --- Test 2: create_node basic ---
  console.log('\n[Test 2] create_node basic');
  try {
    const resp = await callTool('create_node', { type: 'Sprite2D', name: 'TestSprite' });
    const r = getToolResult(resp);
    if (r.success && r.path && r.path.includes('TestSprite')) {
      pass('create_node basic', `path=${r.path}, type=${r.type}`);
    } else {
      fail('create_node basic', JSON.stringify(r));
    }
  } catch (e) {
    fail('create_node basic', e.message);
  }

  // --- Test 3: create_node with parent path ---
  console.log('\n[Test 3] create_node with parent path');
  try {
    const resp = await callTool('create_node', { type: 'Node2D', parent_path: 'TestSprite', name: 'Child' });
    const r = getToolResult(resp);
    if (r.success && r.path && r.path.includes('TestSprite/Child')) {
      pass('create_node with parent path', `path=${r.path}`);
    } else {
      fail('create_node with parent path', JSON.stringify(r));
    }
  } catch (e) {
    fail('create_node with parent path', e.message);
  }

  // --- Test 4: create_node with initial properties ---
  console.log('\n[Test 4] create_node with initial properties');
  try {
    const resp = await callTool('create_node', {
      type: 'Sprite2D', name: 'ColorSprite',
      properties: { modulate: '#ff0000', visible: 'false' }
    });
    const r = getToolResult(resp);
    if (r.success) {
      pass('create_node with initial properties', `path=${r.path} (verify modulate=red, visible=false in Inspector)`);
    } else {
      fail('create_node with initial properties', JSON.stringify(r));
    }
  } catch (e) {
    fail('create_node with initial properties', e.message);
  }

  // --- Test 5: set_node_property position (Vector2) ---
  console.log('\n[Test 5] set_node_property position (Vector2 parsing)');
  try {
    const resp = await callTool('set_node_property', {
      node_path: 'TestSprite', property: 'position', value: 'Vector2(100, 200)'
    });
    const r = getToolResult(resp);
    if (r.success) {
      pass('set_node_property position', '(verify position=(100,200) in Inspector)');
    } else {
      fail('set_node_property position', JSON.stringify(r));
    }
  } catch (e) {
    fail('set_node_property position', e.message);
  }

  // --- Test 6: set_node_property visibility (bool) ---
  console.log('\n[Test 6] set_node_property visibility (bool parsing)');
  try {
    const resp = await callTool('set_node_property', {
      node_path: 'TestSprite', property: 'visible', value: 'false'
    });
    const r = getToolResult(resp);
    if (r.success) {
      pass('set_node_property visibility', '(verify hidden in Scene panel)');
    } else {
      fail('set_node_property visibility', JSON.stringify(r));
    }
  } catch (e) {
    fail('set_node_property visibility', e.message);
  }

  // --- Test 7: delete_node ---
  console.log('\n[Test 7] delete_node');
  try {
    const resp = await callTool('delete_node', { node_path: 'TestSprite/Child' });
    const r = getToolResult(resp);
    if (r.success) {
      pass('delete_node', '(verify Child removed from Scene panel)');
    } else {
      fail('delete_node', JSON.stringify(r));
    }
  } catch (e) {
    fail('delete_node', e.message);
  }

  // --- Verify deletion with get_scene_tree ---
  console.log('\n[Test 7b] Verify Child deleted via get_scene_tree');
  try {
    const resp = await callTool('get_scene_tree', {});
    const r = getToolResult(resp);
    const tree = JSON.stringify(r);
    if (!tree.includes('"Child"') && !tree.includes('"name":"Child"')) {
      pass('Verify Child deleted', 'Child not found in scene tree');
    } else {
      fail('Verify Child deleted', 'Child still found in scene tree');
    }
  } catch (e) {
    fail('Verify Child deleted', e.message);
  }

  // --- Test 8: delete_node scene root protection ---
  console.log('\n[Test 8] delete_node scene root protection');
  try {
    // Get scene root name first
    const treeResp = await callTool('get_scene_tree', {});
    const tree = getToolResult(treeResp);
    const rootName = tree.name || tree.root || 'TestRoot';

    const resp = await callTool('delete_node', { node_path: rootName });
    const r = getToolResult(resp);
    if (r.error && r.error.toLowerCase().includes('scene root')) {
      pass('delete_node scene root protection', `error: ${r.error}`);
    } else if (r.error) {
      pass('delete_node scene root protection', `error (different wording): ${r.error}`);
    } else {
      fail('delete_node scene root protection', `Expected error, got: ${JSON.stringify(r)}`);
    }
  } catch (e) {
    fail('delete_node scene root protection', e.message);
  }

  // --- Test 9: Undo/Redo (cannot automate) ---
  console.log('\n[Test 9] Undo/Redo');
  skip('Undo/Redo', 'Requires manual Ctrl+Z/Y in Godot editor');

  // --- Test 10: Invalid class error ---
  console.log('\n[Test 10] Invalid class error');
  try {
    const resp = await callTool('create_node', { type: 'FakeNodeType' });
    const r = getToolResult(resp);
    if (r.error && (r.error.toLowerCase().includes('unknown class') || r.error.toLowerCase().includes('not') || r.error.toLowerCase().includes('invalid'))) {
      pass('Invalid class error', `error: ${r.error}`);
    } else {
      fail('Invalid class error', `Expected error, got: ${JSON.stringify(r)}`);
    }
  } catch (e) {
    fail('Invalid class error', e.message);
  }

  // --- Test 11: Node not found error ---
  console.log('\n[Test 11] Node not found error');
  try {
    const resp = await callTool('set_node_property', {
      node_path: 'NonExistent', property: 'visible', value: 'true'
    });
    const r = getToolResult(resp);
    if (r.error && (r.error.toLowerCase().includes('not found') || r.error.toLowerCase().includes('node'))) {
      pass('Node not found error', `error: ${r.error}`);
    } else {
      fail('Node not found error', `Expected error, got: ${JSON.stringify(r)}`);
    }
  } catch (e) {
    fail('Node not found error', e.message);
  }

  // --- Summary ---
  console.log('\n=== UAT Summary ===\n');
  const passed = results.filter(r => r.status === 'pass').length;
  const failed = results.filter(r => r.status === 'FAIL').length;
  const skipped = results.filter(r => r.status === 'skip').length;
  console.log(`  Passed:  ${passed}`);
  console.log(`  Failed:  ${failed}`);
  console.log(`  Skipped: ${skipped}`);
  console.log(`  Total:   ${results.length}`);

  if (failed > 0) {
    console.log('\n  Failed tests:');
    results.filter(r => r.status === 'FAIL').forEach(r => {
      console.log(`    - ${r.name}: ${r.detail}`);
    });
  }

  console.log('\n=== JSON Results ===');
  console.log(JSON.stringify(results, null, 2));
}

// Connect and run
client = new net.Socket();
client.connect(PORT, HOST, () => {
  console.log(`Connected to MCP server at ${HOST}:${PORT}`);
  runTests().then(() => {
    client.destroy();
    process.exit(0);
  }).catch(err => {
    console.error('Test runner error:', err);
    client.destroy();
    process.exit(1);
  });
});

client.on('data', (data) => {
  buffer += data.toString();
  // Process newline-delimited JSON
  let newlineIdx;
  while ((newlineIdx = buffer.indexOf('\n')) !== -1) {
    const line = buffer.slice(0, newlineIdx).trim();
    buffer = buffer.slice(newlineIdx + 1);
    if (line && responseResolve) {
      try {
        const parsed = JSON.parse(line);
        const resolve = responseResolve;
        responseResolve = null;
        resolve(parsed);
      } catch (e) {
        // ignore parse errors in buffer
      }
    }
  }
});

client.on('error', (err) => {
  console.error('Connection error:', err.message);
  console.error('Make sure Godot editor is running with the MCP plugin enabled on port 6800.');
  process.exit(1);
});

client.on('close', () => {
  console.log('\nConnection closed.');
});
