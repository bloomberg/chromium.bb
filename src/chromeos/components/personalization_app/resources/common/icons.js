// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Icons specific to personalization app.
 * This file is run in both trusted and untrusted code, and therefore
 * cannot import polymer and iron-iconset-svg itself. Any consumer should
 * import necessary dependencies before this file.
 *
 * These icons should have transparent fill color to adapt to its container's
 * light/dark theme.
 *
 * Following the demo here:
 * @see https://github.com/PolymerElements/iron-iconset-svg/blob/v3.0.1/demo/svg-sample-icons.js
 */

 const template = document.createElement('template');

 template.innerHTML = `
 <iron-iconset-svg name="personalization" size="20">
   <svg>
     <defs>
       <g id="checkmark">
         <!-- this is visually identical to os-settings:ic-checked-filled, but
          that icon is not accessible from chrome-untrusted://personalization.

          We need to apply styling for this icon as it is also displayed in the
          photo container which does not have its own light/dark styling.
         -->
         <style>
           circle {
             fill: white;
           }
           @media (prefers-color-scheme: dark) {
             circle {
               fill: black;
             }
           }
           path {
             fill: var(--cros-icon-color-prominent);
           }
         </style>
         <circle cx="10" cy="10" r="8"/>
         <path d="M10 1.66666C5.40001 1.66666 1.66667 5.39999 1.66667 9.99999C1.66667 14.6 5.40001 18.3333 10 18.3333C14.6 18.3333 18.3333 14.6 18.3333 9.99999C18.3333 5.39999 14.6 1.66666 10 1.66666ZM8.33334 14.1667L5.00001 10.8333L6.16667 9.66666L8.33334 11.8333L13.8333 6.33332L15 7.49999L8.33334 14.1667Z"/>
       </g>
       <g id="change-daily">
         <rect width="20" height="20" fill="none"/>
         <path d="M17.4989 4.16672H19.1656V15.8334H17.4989V4.16672ZM14.1656
         4.16672H15.8322V15.8334H14.1656V4.16672ZM11.6656
         4.16672H1.66558C1.20724 4.16672 0.832245 4.54172 0.832245
         5.00005V15.0001C0.832245 15.4584 1.20724 15.8334 1.66558
         15.8334H11.6656C12.1239 15.8334 12.4989 15.4584 12.4989
         15.0001V5.00005C12.4989 4.54172 12.1239 4.16672 11.6656
         4.16672ZM10.8322 14.1667H2.49891V5.83338H10.8322V14.1667ZM8.00724
         9.37505L6.04058 11.7084L4.79058 10.0001L2.91558
         12.5001H10.4156L8.00724 9.37505Z"/>
       </g>
       <g id="refresh">
         <rect width="20" height="20" fill="none"/>
         <path d="M10 3C6.136 3 3 6.136 3 10C3 13.864 6.136 17 10 17C12.1865 17
         14.1399 15.9959 15.4239 14.4239L13.9984 12.9984C13.0852 14.2129
         11.6325 15 10 15C7.24375 15 5 12.7563 5 10C5 7.24375 7.24375 5 10
         5C11.6318 5 13.0839 5.78641 13.9972 7H11V9H17V3H15V5.10253C13.7292
         3.80529 11.9581 3 10 3Z"/>
       </g>
       <g id="layout_fill">
         <rect width="20" height="20" fill="none"/>
         <path fill-rule="evenodd" clip-rule="evenodd" d="M3.75
         13.125H16.25V6.875L3.75 6.78571V13.125ZM2.5
         14.375H17.5V5.625H2.5V14.375ZM12.5 4.375H14.375V2.5H12.5V4.375ZM12.5
         17.5H14.375V15.625H12.5V17.5ZM15.625 17.5C16.6562 17.5 17.5 16.6562 17.5
         15.625H15.625V17.5ZM8.75 17.5H10.625V15.625H8.75V17.5ZM5.625
         4.375H7.5V2.5H5.625V4.375ZM4.375 17.5V15.625H2.5C2.5 16.6562 3.34375
         17.5 4.375 17.5ZM15.625 2.5V4.375H17.5C17.5 3.34375 16.6562 2.5 15.625
         2.5ZM8.75 4.375H10.625V2.5H8.75V4.375ZM5.625
         17.5H7.5V15.625H5.625V17.5ZM2.5 4.375H4.375V2.5C3.34375 2.5 2.5 3.34375
         2.5 4.375Z"/>
       </g>
       <g id="layout_center">
         <rect width="20" height="20" fill="none"/>
         <path fill-rule="evenodd" clip-rule="evenodd"
         d="M7.5 12.5H12.5V7.5H7.5V12.5ZM6.25 13.75H13.75V6.25H6.25V13.75ZM12.5
         4.375H14.375V2.5H12.5V4.375ZM11.875
         17.5H13.75V15.625H11.875V17.5ZM15.625
         13.75H17.5V11.875H15.625V13.75ZM15.625 7.5H17.5V5.625H15.625V7.5ZM15.625
         17.5C16.6562 17.5 17.5 16.6562 17.5 15.625H15.625V17.5ZM15.625
         10.625H17.5V8.75H15.625V10.625ZM8.75 17.5H10.625V15.625H8.75V17.5ZM6.25
         4.375H8.125V2.5H6.25V4.375ZM2.5 13.75H4.375V11.875H2.5V13.75ZM4.375
         17.5V15.625H2.5C2.5 16.6562 3.34375 17.5 4.375 17.5ZM15.625
         2.5V4.375H17.5C17.5 3.34375 16.6562 2.5 15.625 2.5ZM9.375
         4.375H11.25V2.5H9.375V4.375ZM2.5 7.5H4.375V5.625H2.5V7.5ZM5.625
         17.5H7.5V15.625H5.625V17.5ZM2.5 10.625H4.375V8.75H2.5V10.625ZM2.5
         4.375H4.375V2.5C3.34375 2.5 2.5 3.34375 2.5 4.375Z"/>
       </g>
     </defs>
   </svg>
 </iron-iconset-svg>`;

 document.head.appendChild(template.content);
