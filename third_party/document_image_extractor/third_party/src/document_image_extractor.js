goog.provide('image.collections.extension.DocumentImageExtractor');

goog.require('goog.Uri');
goog.require('goog.asserts');
goog.require('goog.dom');
goog.require('goog.log');
goog.require('goog.math.Size');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.style');
goog.require('goog.uri.utils');
goog.require('gws.collections.common.Constants');
goog.require('image.collections.extension.AdElementFilter');
goog.require('image.collections.extension.DocumentFeature');
goog.require('image.collections.extension.DocumentFeatureExtractor');
goog.require('image.collections.extension.DocumentImage');
goog.require('image.collections.extension.VisibleElementFilter');

goog.scope(function() {
var AdElementFilter = image.collections.extension.AdElementFilter;
var Constants = gws.collections.common.Constants;
var DocumentFeature = image.collections.extension.DocumentFeature;
var DocumentFeatureExtractor =
    image.collections.extension.DocumentFeatureExtractor;
var DocumentImage = image.collections.extension.DocumentImage;
var CustomAttribute = DocumentImage.CustomAttribute;
var VisibleElementFilter = image.collections.extension.VisibleElementFilter;



/**
 * This class is used for extracting a salient image from an HTML document.
 * @extends {DocumentFeatureExtractor}
 * @constructor
 */
image.collections.extension.DocumentImageExtractor = function() {
  DocumentImageExtractor.base(this, 'constructor');

  this.addFilter(new AdElementFilter());
  this.addFilter(new VisibleElementFilter());
};
goog.inherits(image.collections.extension.DocumentImageExtractor,
    DocumentFeatureExtractor);
var DocumentImageExtractor = image.collections.extension.DocumentImageExtractor;
goog.addSingletonGetter(DocumentImageExtractor);


/** @private {goog.log.Logger} Extractor logger. */
DocumentImageExtractor.logger_ = goog.log.getLogger(
    'image.collections.extension.DocumentImageExtractor');


/** @enum {number} */
DocumentImageExtractor.Parameters = {
  AREA_MULTIPLIER: -1e-5,
  ASPECT_RATIO_DEMOTION_FACTOR: 0.8,
  MAX_ASPECT_RATIO: 2,
  MAX_ELEMENTS_WITH_BACKGROUND: 30,
  MAX_OFFSET: 2000,
  NON_TOPLEVEL_DEMOTION_FACTOR: 0.5,
  OFFSET_MULTIPLIER: 1e-3,
  WEIGHT_APPLE: 0.7,
  WEIGHT_MICRODATA: 0.55,
  WEIGHT_MICROSOFT: 0.9,
  WEIGHT_OPEN_GRAPH: 1.0,
  WEIGHT_SRC: 0.6,
  WEIGHT_TWITTER: 0.8
};
var Parameters = DocumentImageExtractor.Parameters;


/**
 * Map of image type to relevance multiplier.
 * @private {!Object.<string, number>}
 */
DocumentImageExtractor.IMAGE_TYPE_RELEVANCE_MULTIPLIER_ =
    goog.object.create('.gif', 0.5);



/** @constructor */
DocumentImageExtractor.Context = function() {
  /** @type {number} */
  this.numElementsWithBackground = 0;

  /** @type {!Object.<string, number>} */
  this.urlToRelevance = {};
};
var Context = DocumentImageExtractor.Context;


/**
 * Extracts salient images from an HTML document.
 * @param {!Document} doc
 * @return {!Array.<!DocumentImage>}
 * @override
 */
DocumentImageExtractor.prototype.extractAllFromDocument = function(doc) {
  var context = new Context();
  return this.extractFromNodeList(doc.getElementsByTagName('*'), context);
};


/**
 * Extracts a salient image from an HTML element, returns null if failed.
 * @param {!Element} element HTML element.
 * @param {Object=} opt_context Optional context.
 * @return {DocumentImage}
 * @override
 */
DocumentImageExtractor.prototype.extractFromElement = function(
    element, opt_context) {
  var image = null;
  switch (element.tagName.toLowerCase()) {
    case 'meta':
      image = this.extractOpenGraphImage_(element) ||
          this.extractMicrosoftImage_(element) ||
          this.extractTwitterImage_(element) ||
          this.extractMicrodataImage_(element);
      break;
    case 'link':
      image = this.extractAppleImage_(element) ||
          this.extractImageSrcImage_(element) ||
          this.extractMicrodataImage_(element);
      break;
    case 'img':
      if (this.filter(element)) {
        image = this.extractImage_(element);
      }
      break;
    default:
      if (this.filter(element) && opt_context.numElementsWithBackground <
          Parameters.MAX_ELEMENTS_WITH_BACKGROUND) {
        image = this.extractBackgroundImage_(element);
        if (image) {
          ++opt_context.numElementsWithBackground;
        }
      }
  }

  if (!image) {
    return null;
  }

  var size = image.getDisplaySize() || image.getSize();
  goog.asserts.assert(!goog.isNull(size));
  if (image.getUrl() != document.location.href) {
    // Ignore images that are too small.
    if (size.width < Constants.EXTRACT_MIN_WIDTH ||
        size.height < Constants.EXTRACT_MIN_HEIGHT) {
      return null;
    }
  }
  // Demote smaller images (squash area using the sigmoid function).
  var relevance = image.getRelevance();
  relevance /= (1 + Math.exp(Parameters.AREA_MULTIPLIER * size.area()));
  // Demote images with bad aspect ratio.
  var aspectRatio = size.aspectRatio();
  if (aspectRatio < 1) {
    aspectRatio = 1 / aspectRatio;
  }
  if (aspectRatio > Parameters.MAX_ASPECT_RATIO) {
    relevance *= Parameters.ASPECT_RATIO_DEMOTION_FACTOR;
  }

  // TODO(busaryev): use the following features:
  // - relative size of the image comparing to neighbors;
  // - area of the visible portion of the image;
  // - position (demote images on the border of the page).

  var url = image.getUrl();
  try {
    // Make sure that image url is absolute.
    var documentUrl = goog.dom.getOwnerDocument(element).documentURI;
    url = goog.Uri.resolve(documentUrl, url).toString();
  } catch (e) {
    goog.log.info(DocumentImageExtractor.logger_,
        'Cannot resolve url: ' + url);
    return null;
  }

  var imagePath = goog.string.makeSafe(goog.uri.utils.getPath(url));
  var lastDot = imagePath.lastIndexOf('.');
  if (lastDot > 0) {
    var imageType = imagePath.slice(lastDot);
    var multiplier =
        DocumentImageExtractor.IMAGE_TYPE_RELEVANCE_MULTIPLIER_[imageType];
    if (multiplier) {
      relevance *= multiplier;
    }
  }

  if (url in opt_context.urlToRelevance &&
      opt_context.urlToRelevance[url] > relevance) {
    return null;
  }
  opt_context.urlToRelevance[url] = relevance;
  return new DocumentImage(relevance, url, image.getSize(),
      image.getDisplaySize());
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractOpenGraphImage_ = function(element) {
  return this.extractCanonicalImage_(
      element, Parameters.WEIGHT_OPEN_GRAPH, 'property', 'og:image', 'content');
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractMicrosoftImage_ = function(element) {
  return this.extractCanonicalImage_(element, Parameters.WEIGHT_MICROSOFT,
      'name', 'msapplication-tileimage', 'content');
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractTwitterImage_ = function(element) {
  return this.extractCanonicalImage_(
      element, Parameters.WEIGHT_TWITTER, 'name', 'twitter:image', 'content');
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractAppleImage_ = function(element) {
  return this.extractCanonicalImage_(
      element, Parameters.WEIGHT_APPLE, 'rel', 'apple-touch-icon', 'href');
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractImageSrcImage_ = function(element) {
  return this.extractCanonicalImage_(
      element, Parameters.WEIGHT_SRC, 'rel', 'image_src', 'href');
};


/**
 * @param {!Element} element
 * @param {!number} relevance
 * @param {string} attributeName
 * @param {string} attribute
 * @param {string} urlAttributeName
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractCanonicalImage_ = function(
    element, relevance, attributeName, attribute, urlAttributeName) {
  goog.asserts.assert(goog.isNumber(relevance));
  goog.asserts.assert(goog.isString(attributeName));
  goog.asserts.assert(goog.isString(attribute));

  if (goog.string.caseInsensitiveEquals(
      goog.string.makeSafe(element.getAttribute(attributeName)), attribute)) {
    var url = element.getAttribute(urlAttributeName);
    if (!url || goog.string.startsWith(url, 'data:')) {
      return null;
    }
    var width = goog.string.parseInt(
        element.getAttribute(CustomAttribute.WIDTH));
    var height = goog.string.parseInt(
        element.getAttribute(CustomAttribute.HEIGHT));
    if (width && height) {
      // For non-toplevel urls, demote the image if it is not in the document.
      var ownerDocument = goog.dom.getOwnerDocument(element);
      if (goog.uri.utils.getPath(ownerDocument.documentURI) != '/' &&
          ownerDocument.body.innerHTML.indexOf(url) == -1) {
        relevance *= Parameters.NON_TOPLEVEL_DEMOTION_FACTOR;
      }
      var size = new goog.math.Size(width, height);
      return new DocumentImage(relevance, url, size);
    }
  }
  return null;
};


/**
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractMicrodataImage_ = function(element) {
  var itemProp = element.getAttribute('itemprop');
  if (itemProp && itemProp.toLowerCase() == 'thumbnailurl') {
    var url = element.getAttribute('href') || element.getAttribute('content');
    if (!url || goog.string.startsWith(url, 'data:')) {
      return null;
    }
    var width = goog.string.parseInt(
        element.getAttribute(CustomAttribute.WIDTH));
    var height = goog.string.parseInt(
        element.getAttribute(CustomAttribute.HEIGHT));
    if (width && height) {
      var size = new goog.math.Size(width, height);
      return new DocumentImage(Parameters.WEIGHT_MICRODATA, url, size);
    }
  }
  return null;
};


/**
 * @param {!Element} element
 * @return {number}
 * @private
 */
DocumentImageExtractor.prototype.getElementRelevance_ = function(element) {
  var offset = goog.style.getPageOffsetTop(element);
  if (offset > Parameters.MAX_OFFSET) {
    return 0;
  }
  return 1 / (1 + Math.exp(Parameters.OFFSET_MULTIPLIER * offset));
};


/**
 * Extracts an image from the <img> HTML element.
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractImage_ = function(element) {
  goog.asserts.assert(element.tagName.toLowerCase() == 'img');
  var url = element.src;
  // We cannot handle data URIs.
  if (url && !goog.string.startsWith(url, 'data:')) {
    var naturalSize = new goog.math.Size(
        element.naturalWidth, element.naturalHeight);
    var displaySize = goog.style.getSize(element);
    var size = naturalSize.area() < displaySize.area() ?
        naturalSize : displaySize;
    if (size.width && size.height) {
      var relevance = this.getElementRelevance_(element);
      return new DocumentImage(relevance, url, naturalSize, displaySize);
    }
  }
  return null;
};


/**
 * Extracts an image specified in 'background-image' property of an element.
 * @param {!Element} element
 * @return {DocumentImage}
 * @private
 */
DocumentImageExtractor.prototype.extractBackgroundImage_ = function(element) {
  var backgroundImage = goog.style.getComputedStyle(
      element, 'background-image');
  var backgroundRepeat = goog.style.getComputedStyle(
      element, 'background-repeat');
  var backgroundSize = goog.style.getComputedStyle(
      element, 'background-size');
  if (backgroundImage &&
      (backgroundRepeat == 'no-repeat' || backgroundSize == 'cover') &&
      goog.string.startsWith(backgroundImage, 'url(') &&
      goog.string.endsWith(backgroundImage, ')')) {
    var url = backgroundImage.substr(4, backgroundImage.length - 5);
    if (url && !goog.string.startsWith(url, 'data:')) {
      var size = goog.style.getSize(element);
      if (size.width && size.height) {
        var relevance = this.getElementRelevance_(element);
        var children = goog.dom.getChildren(element);
        for (var i = 0; i < children.length; ++i) {
          var child = children[i];
          if (goog.style.getComputedStyle(child, 'display') != 'none' &&
              goog.style.getSize(child).area() > 0.1 * size.area()) {
            relevance *= 0.1;
            break;
          }
        }
        return new DocumentImage(relevance, url,
            undefined /* image size is unknown */, size);
      }
    }
  }
  return null;
};
});  // goog.scope
