function createDeepDiv(nest) {
  const x = document.createElement('div');
  if (nest > 0)
    x.appendChild(createDeepDiv(nest - 1));
  return x;
}

function createDeepComponent(nest) {
  // Creating a nested component where a node is re-distributed into a slot in
  // a despendant shadow tree
  const div = document.createElement('div');
  div.appendChild(document.createElement('slot'));
  div.appendChild(document.createElement('p'));
  const shadowRoot = div.attachShadow({ mode: 'open' });
  if (nest == 0) {
    shadowRoot.appendChild(document.createElement('slot'));
  } else {
    shadowRoot.appendChild(createDeepComponent(nest - 1));
  }
  return div;
}

function createHostTree(hostChildren) {
  return createHostTreeWith({
    hostChildren,
    createChildFunction: () => document.createElement('div'),
  });
}

function createHostTreeWithDeepComponentChild(hostChildren) {
  return createHostTreeWith({
    hostChildren,
    createChildFunction: () => createDeepComponent(100),
  });
}

function createHostTreeWith({hostChildren, createChildFunction}) {
  const host = document.createElement('div');
  host.id = 'host';
  for (let i = 0; i < hostChildren; ++i) {
    const div = createChildFunction();
    host.appendChild(div);
  }

  const shadowRoot = host.attachShadow({ mode: 'open' });
  shadowRoot.appendChild(document.createElement('slot'));
  return host;
}

function runHostChildrenMutationThenGetDistribution(host, loop) {
  const slot = host.shadowRoot.querySelector('slot');
  for (let i = 0; i < loop; ++i) {
    const firstChild = host.firstChild;
    firstChild.remove();
    host.appendChild(firstChild);
    slot.assignedNodes({ flatten: true });
  }
}

function runHostChildrenMutationThenLayout(host, loop) {
  for (let i = 0; i < loop; ++i) {
    const firstChild = host.firstChild;
    firstChild.remove();
    host.appendChild(firstChild);
    PerfTestRunner.forceLayout();
  }
}
