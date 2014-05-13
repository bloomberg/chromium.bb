// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/host_list_view_controller.h"

#import "remoting/ios/authorize.h"
#import "remoting/ios/host.h"
#import "remoting/ios/host_cell.h"
#import "remoting/ios/host_refresh.h"
#import "remoting/ios/utility.h"
#import "remoting/ios/ui/host_view_controller.h"

@interface HostListViewController (Private)
- (void)refreshHostList;
- (void)checkUserAndRefreshHostList;
- (BOOL)isSignedIn;
- (void)signInUser;
// Callback from [Authorize createLoginController...]
- (void)viewController:(UIViewController*)viewController
      finishedWithAuth:(GTMOAuth2Authentication*)authResult
                 error:(NSError*)error;
@end

@implementation HostListViewController

@synthesize userEmail = _userEmail;
@synthesize authorization = _authorization;

// Override default setter
- (void)setAuthorization:(GTMOAuth2Authentication*)authorization {
  _authorization = authorization;
  if (_authorization.canAuthorize) {
    _userEmail = _authorization.userEmail;
  } else {
    _userEmail = nil;
  }

  NSString* userName = _userEmail;

  if (userName == nil) {
    userName = @"Not logged in";
  }

  [_btnAccount setTitle:userName forState:UIControlStateNormal];

  [self refreshHostList];
}

// Override UIViewController
// Create google+ service for google authentication and oAuth2 authorization.
- (void)viewDidLoad {
  [super viewDidLoad];

  [_tableHostList setDataSource:self];
  [_tableHostList setDelegate:self];

  _versionInfo.title = [Utility appVersionNumberDisplayString];
}

// Override UIViewController
- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:NO animated:NO];
  [self setAuthorization:[Authorize getAnyExistingAuthorization]];
}

// Override UIViewController
// Cancel segue when host status is not online
- (BOOL)shouldPerformSegueWithIdentifier:(NSString*)identifier
                                  sender:(id)sender {
  if ([identifier isEqualToString:@"ConnectToHost"]) {
    Host* host = [self hostAtIndex:[_tableHostList indexPathForCell:sender]];
    if (![host.status isEqualToString:@"ONLINE"]) {
      return NO;
    }
  }
  return YES;
}

// Override UIViewController
// check for segues defined in the storyboard by identifier, and set a few
// properties before transitioning
- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender {
  if ([segue.identifier isEqualToString:@"ConnectToHost"]) {
    // the designationViewController type is defined by the storyboard
    HostViewController* hostView =
        static_cast<HostViewController*>(segue.destinationViewController);

    NSString* authToken =
        [_authorization.parameters valueForKey:@"access_token"];

    if (authToken == nil) {
      authToken = _authorization.authorizationTokenKey;
    }

    [hostView setHostDetails:[self hostAtIndex:[_tableHostList
                                                   indexPathForCell:sender]]
                   userEmail:_userEmail
          authorizationToken:authToken];
  }
}

// @protocol HostRefreshDelegate, remember received host list for the table
// view to refresh from
- (void)hostListRefresh:(NSArray*)hostList
           errorMessage:(NSString*)errorMessage {
  if (hostList != nil) {
    _hostList = hostList;
    [_tableHostList reloadData];
  }
  [_refreshActivityIndicator stopAnimating];
  if (errorMessage != nil) {
    [Utility showAlert:@"Host Refresh Failed" message:errorMessage];
  }
}

// @protocol UITableViewDataSource
// Only have 1 section and it contains all the hosts
- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [_hostList count];
}

// @protocol UITableViewDataSource
// Convert a host entry to a table row
- (HostCell*)tableView:(UITableView*)tableView
    cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  static NSString* CellIdentifier = @"HostStatusCell";

  HostCell* cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier
                                                   forIndexPath:indexPath];

  Host* host = [self hostAtIndex:indexPath];
  cell.labelHostName.text = host.hostName;
  cell.labelStatus.text = host.status;

  UIColor* statColor = nil;
  if ([host.status isEqualToString:@"ONLINE"]) {
    statColor = [[UIColor alloc] initWithRed:0 green:1 blue:0 alpha:1];
  } else {
    statColor = [[UIColor alloc] initWithRed:1 green:0 blue:0 alpha:1];
  }
  [cell.labelStatus setTextColor:statColor];

  return cell;
}

// @protocol UITableViewDataSource
// Rows are not editable via standard UI mechanisms
- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (IBAction)btnRefreshHostListPressed:(id)sender {
  [self refreshHostList];
}

- (IBAction)btnAccountPressed:(id)sender {
  [self signInUser];
}

- (void)refreshHostList {
  [_refreshActivityIndicator startAnimating];
  _hostList = [[NSArray alloc] init];
  [_tableHostList reloadData];

  // Insert a small delay so the user is well informed that something is
  // happening by the animating activity indicator
  [self performSelector:@selector(checkUserAndRefreshHostList)
             withObject:nil
             afterDelay:.5];
}

//  Most likely you want to call refreshHostList
- (void)checkUserAndRefreshHostList {
  if (![self isSignedIn]) {
    [self signInUser];
  } else {
    HostRefresh* hostRefresh = [[HostRefresh alloc] init];
    [hostRefresh refreshHostList:_authorization delegate:self];
  }
}

- (BOOL)isSignedIn {
  return (_userEmail != nil);
}

// Launch the google.com authentication and authorization process.  If a user is
// already signed in, begin by signing out so another account could be
// signed in.
- (void)signInUser {
  [self presentViewController:
            [Authorize createLoginController:self
                            finishedSelector:@selector(viewController:
                                                     finishedWithAuth:
                                                                error:)]
                     animated:YES
                   completion:nil];
}

// Callback from [Authorize createLoginController...]
// Handle completion of the authentication process, and updates the service
// with the new credentials.
- (void)viewController:(UIViewController*)viewController
      finishedWithAuth:(GTMOAuth2Authentication*)authResult
                 error:(NSError*)error {
  [viewController.presentingViewController dismissViewControllerAnimated:NO
                                                              completion:nil];

  if (error != nil) {
    [Utility showAlert:@"Authentication Error"
               message:error.localizedDescription];
    [self setAuthorization:nil];
  } else {
    [self setAuthorization:authResult];
  }
}

- (Host*)hostAtIndex:(NSIndexPath*)indexPath {
  return [_hostList objectAtIndex:indexPath.row];
}

@end
