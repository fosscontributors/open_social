<?php

namespace Drupal\social_core\Routing;

use Drupal\Core\Extension\ModuleHandlerInterface;
use Drupal\Core\Routing\RouteSubscriberBase;
use Drupal\social_core\Controller\EntityAutocompleteController;
use Drupal\social_core\Controller\SocialCoreController;
use Symfony\Component\Routing\RouteCollection;

/**
 * Listens to the dynamic route events.
 */
class RouteSubscriber extends RouteSubscriberBase {

  /**
   * The list of methods overrides page titles.
   */
  private const CALLBACKS = [
    'system.entity_autocomplete' => EntityAutocompleteController::class . '::handleAutocomplete',
  ];

  /**
   * The module handler.
   */
  private ModuleHandlerInterface $moduleHandler;

  /**
   * RouteSubscriber constructor.
   *
   * @param \Drupal\Core\Extension\ModuleHandlerInterface $module_handler
   *   The module handler.
   */
  public function __construct(ModuleHandlerInterface $module_handler) {
    $this->moduleHandler = $module_handler;
  }

  /**
   * {@inheritdoc}
   */
  protected function alterRoutes(RouteCollection $collection) {
    foreach (self::CALLBACKS as $route_name => $callback) {
      if ($route = $collection->get($route_name)) {
        $route->setDefault('_controller', $callback);
      }
    }

    $titles = $this->moduleHandler->invokeAll('social_core_title');
    $this->moduleHandler->alter('social_core_title', $titles);

    if (!empty($titles['node'])) {
      if (!isset($titles['node']['bundles'])) {
        $titles['node']['bundles'] = [];
      }
    }

    // Write our own page title resolver for creation pages.
    foreach (array_column($titles, 'route_name') as $route_name) {
      if ($route = $collection->get($route_name)) {
        $route->setDefault(
          '_title_callback',
          SocialCoreController::class . '::addPageTitle',
        );
      }
    }
  }

}
