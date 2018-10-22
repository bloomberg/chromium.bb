'use strict';
/**
 * Concrete implementation of Plotter strategy for creating
 * box plots.
 * @implements {Plotter}
 */
class DotPlotter {
  constructor() {
    this.radius_ = 4;
    /** @private @const {Object} */
    this.scaleForXAxis_ = undefined;
    /** @private @const {Object} */
    this.scaleForYAxis_ = undefined;
    /** @private @const {Object} */
    this.xAxisGenerator_ = undefined;
    /** @private @const {Object} */
    this.xAxisDrawing_ = undefined;
  }
  /**
   * Initalises the chart by computing the scales for the axes and
   * drawing them. It also applies the labels to the graph (axes and
   * graph titles).
   * @param {GraphData} graph Data to be drawn.
   * @param {Object} chart Chart to draw to.
   * @param {Object} chartDimensions Size of the chart.
   */
  initChart_(graph, chart, chartDimensions) {
    this.scaleForXAxis_ = this.createXAxisScale_(graph, chartDimensions);
    this.scaleForYAxis_ = this.createYAxisScale_(graph, chartDimensions);
    this.xAxisGenerator_ = d3.axisBottom(this.scaleForXAxis_);
    // Draw the x-axis.
    this.xAxisDrawing_ = chart.append('g')
        .call(this.xAxisGenerator_)
        .attr('transform', `translate(0, ${chartDimensions.height})`);
  }

  createXAxisScale_(graph, chartDimensions) {
    return d3.scaleLinear()
        .domain([0, graph.max(x => x)])
        .range([0, chartDimensions.width]);
  }

  createYAxisScale_(graph, chartDimensions) {
    const categories = graph.keys();
    // The gap allows for padding between the first and last categories and
    // the top and bottom of the chart area.
    const gaps = graph.dataSources.length + 1;
    const gapSize = chartDimensions.height / gaps;
    return d3.scaleOrdinal()
        .domain(categories)
        .range([gapSize, chartDimensions.height - gapSize]);
  }

  getDotDiameter_() {
    return this.radius_ * 2;
  }

  dotOffset_(stackOffset, key) {
    return this.scaleForYAxis_(key) - stackOffset * this.getDotDiameter_();
  }

  /**
   * Collects the data into groups so that dots which would otherwise overlap
   * are stacked on top of each other instead. This is a rough implementation
   * and allows occasional overlap in some cases, where
   * values in adjacent bins overlap. However, in this case the overlap is
   * bounded to very few elements.
   */
  computeDotStacking_(data, xAxisScale) {
    const bins = {};
    // Each bin corresponds to the size of the diameter of a dot,
    // so any data points in the same bin will definitely overlap.
    const binSize =
        xAxisScale.invert(this.getDotDiameter_()) - xAxisScale.invert(0);
    data.forEach((datum, id) => {
      // The lower bound of the bin will be some multiple of binSize so find
      // the closest such multiple less than the datum.
      const lowerBound = Math.round(datum) - (Math.round(datum) % binSize);
      const binMid = lowerBound + binSize / 2;
      if (!bins[binMid]) {
        bins[binMid] = [];
      }
      bins[binMid].push({
        datum,
        id,
      });
    });
    const newPositions = [];
    // Give each value an offset so that they do not overlap.
    Object.values(bins).forEach(bin => {
      const binSize = bin.length;
      let stackOffset = - Math.floor(binSize / 2);
      if (stackOffset === -0) {
        stackOffset = 0;
      }
      bin.forEach(({ datum, id }) => {
        newPositions.push({
          x: datum,
          stackOffset,
          id,
        });
        stackOffset++;
      });
    });
    return newPositions;
  }

  /**
   * Draws a dot plot to the canvas. If there are multiple dataSources it will
   * plot them both and label their colors in the legend.
   * @param {GraphData} graph The data to be plotted.
   * @param {Object} chart d3 selection for the chart element to be drawn on.
   * @param {Object} legend d3 selection for the legend element for
   * additional information to be drawn on.
   * @param {Object} chartDimensions The margins, width and height
   * of the chart. This is useful for computing appropriates axis
   * scales and positioning elements.
   */
  plot(graph, chart, legend, chartDimensions) {
    this.initChart_(graph, chart, chartDimensions);
    const dots = graph.process(
        this.computeDotStacking_.bind(this), this.scaleForXAxis_);
    const getClassNameSuffix = GraphUtils.getClassNameSuffixFactory();
    this.setUpZoom_(graph, chart, chartDimensions, getClassNameSuffix);
    dots.forEach(({ data, color, key }, index) => {
      const className = `dot-${getClassNameSuffix(key)}`;
      const testTargetPrefix = `chai-test-${className}-`;
      chart.append('line')
          .attr('x1', 0)
          .attr('x2', chartDimensions.width)
          .attr('y1', this.scaleForYAxis_(key))
          .attr('y2', this.scaleForYAxis_(key))
          .attr('stroke-width', 2)
          .attr('stroke-dasharray', 4)
          .attr('stroke', 'gray');
      chart.selectAll(className)
          .data(data)
          .enter()
          .append('circle')
          .attr('cx', datum => this.scaleForXAxis_(datum.x))
          .attr('cy', datum => this.dotOffset_(datum.stackOffset, key))
          .attr('r', this.radius_)
          .attr('fill', color)
          .attr('class', datum => `${className} ${testTargetPrefix}${datum.x}`)
          .attr('clip-path', 'url(#plot-clip)')
          .on('click', datum => {
            graph.interactiveCallbackForDotPlot(key, datum.id);
          })
          .on('mouseover', (datum, datumIndex) => {
            const zoomOnHoverScaleFactor = 2;
            d3.selectAll(`.${className}`)
                .filter((d, i) => i === datumIndex)
                .attr('r', this.radius_ * zoomOnHoverScaleFactor);
          })
          .on('mouseout', (datum, datumIndex) => {
            d3.selectAll(`.${className}`)
                .filter((d, i) => i === datumIndex)
                .attr('r', this.radius_);
          })
          .append('title')
          .text('click to view trace');
      legend.append('text')
          .text(key)
          .attr('y', index + 'em')
          .attr('fill', color);
    });
  }

  setUpZoom_(graph, chart, chartDimensions, getClassNameSuffix) {
    const axes = {
      x: {
        generator: this.xAxisGenerator_,
        drawing: this.xAxisDrawing_,
        scale: this.scaleForXAxis_,
      },
    };
    const draw = (xAxisScale) => {
      const dots = graph.process(
          this.computeDotStacking_.bind(this), xAxisScale);
      dots.forEach(({ data, key }) => {
        const newDots = chart
            .selectAll(`.dot-${getClassNameSuffix(key)}`)
            .data(data);
        newDots
            .attr('cx', datum => xAxisScale(datum.x))
            .attr('cy', datum => this.dotOffset_(datum.stackOffset, key));
      });
    };
    const shouldScale = {
      x: true,
      y: false,
    };
    GraphUtils.createZoom(shouldScale, chart, chartDimensions, draw, axes);
  }
}
